/*
 *  Copyright (C) 2015-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include <string>
#include <vector>

#include "ILanguageInvoker.h"
#include "interfaces/generic/ILanguageInvocationHandler.h"

ILanguageInvoker::ILanguageInvoker(ILanguageInvocationHandler *invocationHandler)
  : m_id(-1),
    m_state(InvokerStateUninitialized),
    m_invocationHandler(invocationHandler)
{ }

ILanguageInvoker::~ILanguageInvoker() = default;

bool ILanguageInvoker::Execute(const std::string &script, const std::vector<std::string> &arguments, CleanupParamsPtr *cleanup)
{
  if (m_invocationHandler)
    m_invocationHandler->OnScriptStarted(this);

  return execute(script, arguments, cleanup);
}

bool ILanguageInvoker::Stop(bool abort /* = false */)
{
  return stop(abort);
}

bool ILanguageInvoker::IsActive() const
{
  return GetState() > InvokerStateUninitialized && GetState() < InvokerStateScriptDone;
}

bool ILanguageInvoker::IsRunning() const
{
  return GetState() == InvokerStateRunning;
}

bool ILanguageInvoker::IsStopping() const
{
  return GetState() == InvokerStateStopping;
}

void ILanguageInvoker::pulseGlobalEvent()
{
  if (m_invocationHandler)
    m_invocationHandler->PulseGlobalEvent();
}

bool ILanguageInvoker::onExecutionInitialized()
{
  if (m_invocationHandler == NULL)
    return false;

  return m_invocationHandler->OnScriptInitialized(this);
}

void ILanguageInvoker::onAbortRequested()
{
  if (m_invocationHandler)
    m_invocationHandler->OnScriptAbortRequested(this);
}

void ILanguageInvoker::onExecutionFailed()
{
  if (m_invocationHandler)
    m_invocationHandler->OnExecutionEnded(this);
}

void ILanguageInvoker::onExecutionDone()
{
  if (m_invocationHandler)
    m_invocationHandler->OnExecutionEnded(this);
}

void ILanguageInvoker::onExecutionFinalized()
{
  if (m_invocationHandler)
    m_invocationHandler->OnScriptFinalized(this);
}

void ILanguageInvoker::setState(InvokerState state)
{
  if (state <= m_state)
    return;

  m_state = state;
}

bool ICleanupParams::Load(void *data)
{
  m_cleanup = false;
  m_cleanup_timeouts.clear();
  load(data);
  update_flags();
  return m_flags != 0;
}

bool ICleanupParams::GetCleanupIDs(time_t cur, std::vector<int>& ids)
{
  for (auto& it : m_cleanup_timeouts)
    if (it.second <= cur)
      ids.push_back(it.first);
  if (ids.empty())
    return false;
  for (auto& id : ids)
    m_cleanup_timeouts.erase(id);
  update_flags();
  return true;
}

void ICleanupParams::update_flags()
{
  m_flags = (m_cleanup_timeouts.empty() ? 0 : 2) | (m_cleanup ? 1 : 0);
}
