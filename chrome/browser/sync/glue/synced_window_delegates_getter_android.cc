// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sync_driver/glue/synced_window_delegate.h"

namespace browser_sync {

SyncedWindowDelegatesGetterAndroid::SyncedWindowDelegatesGetterAndroid() {}
SyncedWindowDelegatesGetterAndroid::~SyncedWindowDelegatesGetterAndroid() {}

std::set<const SyncedWindowDelegate*>
SyncedWindowDelegatesGetterAndroid::GetSyncedWindowDelegates() {
  std::set<SyncedWindowDelegate const*> synced_window_delegates;
  for (auto i = TabModelList::begin(); i != TabModelList::end(); ++i) {
    synced_window_delegates.insert((*i)->GetSyncedWindowDelegate());
  }
  return synced_window_delegates;
}

const SyncedWindowDelegate* SyncedWindowDelegatesGetterAndroid::FindById(
    SessionID::id_type session_id) {
  TabModel* tab_model = TabModelList::FindTabModelWithId(session_id);

  // In case we don't find the browser (e.g. for Developer Tools).
  return tab_model ? tab_model->GetSyncedWindowDelegate() : nullptr;
}

}  // namespace browser_sync
