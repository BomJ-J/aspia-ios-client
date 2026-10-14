//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

// iOS implementation of BiometricGate (client/android/biometric_gate.h).
//
// The master key is kept in the keychain under an access control that asks for Face ID or Touch ID
// every time the item is read and that is bound to the biometrics enrolled at the time: adding a
// new face or finger makes the item unreadable, which is reported as KEY_INVALIDATED, the same way
// a key invalidated in the Android keystore is. The blob kept in the database names the item.

#include "client/android/biometric_gate.h"

#include <QEventLoop>
#include <QMetaObject>
#include <QPointer>

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include <functional>
#include <memory>
#include <mutex>

#include "base/logging.h"
#include "base/crypto/random.h"

namespace {

// The keychain service of every biometric item of the application.
NSString* const kKeychainService = @"org.aspia.client.biometric";

// Marks a blob written by this implementation (the Android one stores a GCM IV in its place).
const char kBlobMarker[] = "ios-keychain-v1";

// Length of the random name of a keychain item.
const int kItemNameLength = 16;

// The outcome of a call that finishes on a queue of the system, handed over to the GUI thread.
struct AsyncResult
{
    std::mutex mutex;
    QPointer<QEventLoop> loop;
    BiometricGate::Result result = BiometricGate::Result::FAILED;
    QByteArray data;
};

//--------------------------------------------------------------------------------------------------
// Runs |work| on a background queue and spins a local event loop on the calling (GUI) thread until
// it finishes. The keychain and LocalAuthentication show their prompts themselves and must not be
// waited on from the main thread.
BiometricGate::Result runAndWait(
    const std::function<void(std::shared_ptr<AsyncResult>, std::function<void()>)>& work,
    QByteArray* data)
{
    QEventLoop loop;

    auto state = std::make_shared<AsyncResult>();
    state->loop = &loop;

    std::function<void()> done = [state]()
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->loop)
            QMetaObject::invokeMethod(state->loop.data(), "quit", Qt::QueuedConnection);
    };

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        work(state, done);
    });

    loop.exec();

    std::lock_guard<std::mutex> lock(state->mutex);
    state->loop = nullptr;

    if (data)
        *data = state->data;

    return state->result;
}

//--------------------------------------------------------------------------------------------------
QString errorString(NSError* error)
{
    if (!error)
        return QString();

    return QString::fromNSString([error localizedDescription]);
}

//--------------------------------------------------------------------------------------------------
BiometricGate::Result fromLaError(NSError* error)
{
    if (!error)
        return BiometricGate::Result::FAILED;

    switch ([error code])
    {
        case LAErrorUserCancel:
        case LAErrorSystemCancel:
        case LAErrorAppCancel:
        case LAErrorUserFallback:
            return BiometricGate::Result::CANCELED;

        default:
            return BiometricGate::Result::FAILED;
    }
}

//--------------------------------------------------------------------------------------------------
NSData* toNSData(const QByteArray& bytes)
{
    return [NSData dataWithBytes:bytes.constData() length:static_cast<NSUInteger>(bytes.size())];
}

//--------------------------------------------------------------------------------------------------
NSMutableDictionary* itemQuery(const QByteArray& name)
{
    NSMutableDictionary* query = [NSMutableDictionary dictionary];
    query[(__bridge id)kSecClass] = (__bridge id)kSecClassGenericPassword;
    query[(__bridge id)kSecAttrService] = kKeychainService;
    query[(__bridge id)kSecAttrAccount] = QString::fromLatin1(name.toHex()).toNSString();
    return query;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
BiometricGate::Status BiometricGate::status()
{
    @autoreleasepool
    {
        LAContext* context = [[[LAContext alloc] init] autorelease];
        NSError* error = nil;

        if ([context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error])
            return Status::AVAILABLE;

        switch ([error code])
        {
            case LAErrorBiometryNotEnrolled:
                return Status::NONE_ENROLLED;

            case LAErrorBiometryNotAvailable:
                return Status::NO_HARDWARE;

            default:
                LOG(INFO) << "Biometrics are not usable:" << errorString(error);
                return Status::UNAVAILABLE;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<BiometricGate::Blob> BiometricGate::enroll(const SecureByteArray& key,
                                                         const Prompt& prompt)
{
    // The user confirms the enrollment with the biometrics first, as on Android.
    NSString* reason = [prompt.title.toNSString() retain];
    NSString* cancel = [prompt.negative_text.toNSString() retain];

    Result result = runAndWait([reason, cancel](std::shared_ptr<AsyncResult> state,
                                                std::function<void()> done)
    {
        LAContext* context = [[LAContext alloc] init];
        if ([cancel length] != 0)
            context.localizedCancelTitle = cancel;

        [context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                localizedReason:reason
                          reply:^(BOOL success, NSError* error)
        {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (success)
                {
                    state->result = Result::SUCCESS;
                }
                else
                {
                    state->result = fromLaError(error);
                    LOG(INFO) << "Biometric confirmation failed:" << errorString(error);
                }
            }

            [context release];
            done();
        }];
    }, nullptr);

    [reason release];
    [cancel release];

    if (result != Result::SUCCESS)
        return std::nullopt;

    @autoreleasepool
    {
        CFErrorRef cf_error = nullptr;
        SecAccessControlRef access = SecAccessControlCreateWithFlags(
            kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly,
            kSecAccessControlBiometryCurrentSet, &cf_error);
        if (!access)
        {
            LOG(ERROR) << "SecAccessControlCreateWithFlags failed:"
                       << errorString((__bridge NSError*)cf_error);
            if (cf_error)
                CFRelease(cf_error);
            return std::nullopt;
        }

        const QByteArray name = Random::byteArray(kItemNameLength);

        NSMutableDictionary* attributes = itemQuery(name);
        attributes[(__bridge id)kSecAttrAccessControl] = (__bridge id)access;
        attributes[(__bridge id)kSecValueData] = toNSData(key.toByteArray());

        OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, nullptr);
        CFRelease(access);

        if (status != errSecSuccess)
        {
            LOG(ERROR) << "SecItemAdd failed:" << status;
            return std::nullopt;
        }

        return Blob{ QByteArray(kBlobMarker), name };
    }
}

//--------------------------------------------------------------------------------------------------
// static
BiometricGate::UnlockResult BiometricGate::unlock(const Blob& blob, const Prompt& prompt)
{
    UnlockResult unlock_result;

    if (blob.iv != QByteArray(kBlobMarker) || blob.ciphertext.size() != kItemNameLength)
    {
        // A blob of another format (a database brought over from Android, for example).
        unlock_result.result = Result::KEY_INVALIDATED;
        return unlock_result;
    }

    const QByteArray name = blob.ciphertext;
    NSString* reason = [prompt.title.toNSString() retain];
    NSString* cancel = [prompt.negative_text.toNSString() retain];

    QByteArray data;
    Result result = runAndWait([name, reason, cancel](std::shared_ptr<AsyncResult> state,
                                                      std::function<void()> done)
    {
        @autoreleasepool
        {
            LAContext* context = [[[LAContext alloc] init] autorelease];
            context.localizedReason = reason;
            if ([cancel length] != 0)
                context.localizedCancelTitle = cancel;

            NSMutableDictionary* query = itemQuery(name);
            query[(__bridge id)kSecReturnData] = @YES;
            query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
            query[(__bridge id)kSecUseAuthenticationContext] = context;

            // Blocks until the user passes or dismisses the prompt of the system.
            CFTypeRef found = nullptr;
            OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &found);

            std::lock_guard<std::mutex> lock(state->mutex);

            switch (status)
            {
                case errSecSuccess:
                {
                    NSData* value = (__bridge NSData*)found;
                    state->data = QByteArray(static_cast<const char*>([value bytes]),
                                             static_cast<qsizetype>([value length]));
                    state->result = Result::SUCCESS;
                }
                break;

                case errSecUserCanceled:
                    state->result = Result::CANCELED;
                    break;

                case errSecItemNotFound:
                    // The item is gone or the enrolled biometrics changed since it was added.
                    state->result = Result::KEY_INVALIDATED;
                    break;

                default:
                    LOG(INFO) << "SecItemCopyMatching failed:" << status;
                    state->result = Result::FAILED;
                    break;
            }

            if (found)
                CFRelease(found);
        }

        done();
    }, &data);

    [reason release];
    [cancel release];

    unlock_result.result = result;
    if (result == Result::SUCCESS)
        unlock_result.key = SecureByteArray(std::move(data));

    return unlock_result;
}

//--------------------------------------------------------------------------------------------------
// static
void BiometricGate::deleteKey()
{
    @autoreleasepool
    {
        NSDictionary* query = @{
            (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
            (__bridge id)kSecAttrService : kKeychainService
        };

        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
        if (status != errSecSuccess && status != errSecItemNotFound)
            LOG(ERROR) << "SecItemDelete failed:" << status;
    }
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray BiometricGate::pack(const Blob& blob)
{
    QByteArray packed;
    packed.append(static_cast<char>(blob.iv.size()));
    packed.append(blob.iv);
    packed.append(blob.ciphertext);
    return packed;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<BiometricGate::Blob> BiometricGate::unpack(const QByteArray& packed)
{
    if (packed.isEmpty())
        return std::nullopt;

    const int iv_size = static_cast<quint8>(packed.at(0));
    if (packed.size() < 1 + iv_size)
        return std::nullopt;

    Blob blob;
    blob.iv = packed.mid(1, iv_size);
    blob.ciphertext = packed.mid(1 + iv_size);
    return blob;
}
