// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAMERA_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAMERA_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class MediaStreamDevicePermissionContext;
class Profile;

class MediaStreamCameraPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static MediaStreamDevicePermissionContext* GetForProfile(Profile* profile);
  static MediaStreamCameraPermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<
      MediaStreamCameraPermissionContextFactory>;

  MediaStreamCameraPermissionContextFactory();
  ~MediaStreamCameraPermissionContextFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamCameraPermissionContextFactory);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_STREAM_CAMERA_PERMISSION_CONTEXT_FACTORY_H_
