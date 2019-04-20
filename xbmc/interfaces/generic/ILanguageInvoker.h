/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "addons/IAddon.h"

class CLanguageInvokerThread;
class ILanguageInvocationHandler;

typedef enum {
  InvokerStateUninitialized,
  InvokerStateInitialized,
  InvokerStateRunning,
  InvokerStateStopping,
  InvokerStateScriptDone,
  InvokerStateExecutionDone,
  InvokerStateFailed
} InvokerState;

class ICleanupParams
{
public:
  bool Load(void *data);
  bool NeedCleanup(bool atexit) const { return (m_flags & (atexit ? 3 : 2)) != 0; }
  bool GetCleanupIDs(time_t cur, std::vector<int>& ids);
  virtual std::vector<std::string> GetCleanupArgs(const ADDON::AddonPtr& addon, const std::vector<std::string>& args, const std::vector<int> *ids) = 0;
protected:
  virtual void load(void *data) = 0;
  void update_flags();
  std::atomic<int> m_flags;
  bool m_cleanup;
  std::map<int, time_t> m_cleanup_timeouts;
};

typedef std::unique_ptr<ICleanupParams> CleanupParamsPtr;

class ILanguageInvoker
{
public:
  explicit ILanguageInvoker(ILanguageInvocationHandler *invocationHandler);
  virtual ~ILanguageInvoker();

  virtual bool Execute(const std::string &script, const std::vector<std::string> &arguments = std::vector<std::string>(), CleanupParamsPtr *cleanup = nullptr);
  virtual bool Stop(bool abort = false);
  virtual bool IsStopping() const;

  void SetId(int id) { m_id = id; }
  int GetId() const { return m_id; }
  const ADDON::AddonPtr& GetAddon() const { return m_addon; }
  void SetAddon(const ADDON::AddonPtr &addon) { m_addon = addon; }
  InvokerState GetState() const { return m_state; }
  bool IsActive() const;
  bool IsRunning() const;
  void Reset() { m_state = InvokerStateUninitialized; };

protected:
  friend class CLanguageInvokerThread;

  virtual bool execute(const std::string &script, const std::vector<std::string> &arguments, CleanupParamsPtr *cleanup) = 0;
  virtual bool stop(bool abort) = 0;

  virtual void pulseGlobalEvent();
  virtual bool onExecutionInitialized();
  virtual void onAbortRequested();
  virtual void onExecutionFailed();
  virtual void onExecutionDone();
  virtual void onExecutionFinalized();

  void setState(InvokerState state);

  ADDON::AddonPtr m_addon;

private:
  int m_id;
  InvokerState m_state;
  ILanguageInvocationHandler *m_invocationHandler;
};

typedef std::shared_ptr<ILanguageInvoker> LanguageInvokerPtr;
