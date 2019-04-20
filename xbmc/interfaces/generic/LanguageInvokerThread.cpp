/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "LanguageInvokerThread.h"
#include "ScriptInvocationManager.h"
#include "utils/log.h"

CLanguageInvokerThread::CLanguageInvokerThread(LanguageInvokerPtr invoker, CScriptInvocationManager *invocationManager, bool reuseable)
  : ILanguageInvoker(NULL),
    CThread("LanguageInvoker"),
    m_invoker(invoker),
    m_invocationManager(invocationManager),
    m_reusable(reuseable),
    m_last_check()
{ }

CLanguageInvokerThread::~CLanguageInvokerThread()
{
  Stop(true);
}

InvokerState CLanguageInvokerThread::GetState() const
{
  if (m_invoker == NULL)
    return InvokerStateFailed;

  return m_invoker->GetState();
}

void CLanguageInvokerThread::Release()
{
  m_bStop = true;
  m_condition.notify_one();
}

bool CLanguageInvokerThread::execute(const std::string &script, const std::vector<std::string> &arguments, CleanupParamsPtr *cleanup)
{
  if (m_invoker == NULL || script.empty())
    return false;

  m_script = script;
  m_args = arguments;

  if (CThread::IsRunning())
  {
    std::unique_lock<std::mutex> lck(m_mutex);
    m_restart = true;
    m_condition.notify_one();
  }
  else
    Create();

  //Todo wait until running

  return true;
}

bool CLanguageInvokerThread::stop(bool wait)
{
  if (m_invoker == NULL)
    return false;

  if (!CThread::IsRunning())
    return false;

  Release();

  bool result = true;
  if (m_invoker->GetState() < InvokerStateExecutionDone)
  {
    // stop the language-specific invoker
    result = m_invoker->Stop(wait);
  }
  // stop the thread
  CThread::StopThread(wait);

  return result;
}

void CLanguageInvokerThread::OnStartup()
{
  if (m_invoker == NULL)
    return;

  m_invoker->SetId(GetId());
  if (m_addon != NULL)
    m_invoker->SetAddon(m_addon);
}

void CLanguageInvokerThread::Process()
{
  if (m_invoker == NULL)
    return;

  std::unique_lock<std::mutex> lckdl(m_mutex);
  do {
    m_restart = false;
    if (m_cleanup_ids.empty())
      m_invoker->Execute(m_script, m_args, &m_cleanup_params);
    else
    {
      m_invoker->Execute(m_script, m_cleanup_params->GetCleanupArgs(m_addon, m_args, &m_cleanup_ids));
      m_cleanup_ids.clear();
    }

    if (m_invoker->GetState() != InvokerStateScriptDone)
      m_reusable = false;

    m_condition.wait(lckdl, [this] {return m_bStop || m_restart || !m_reusable; });

  } while (m_reusable && !m_bStop);

  if (m_reusable && m_cleanup_params && m_cleanup_params->NeedCleanup(true))
    m_invoker->Execute(m_script, m_cleanup_params->GetCleanupArgs(m_addon, m_args, nullptr));
}

void CLanguageInvokerThread::OnExit()
{
  if (m_invoker == NULL)
    return;

  m_invoker->onExecutionDone();
  m_invocationManager->OnExecutionDone(GetId());
}

void CLanguageInvokerThread::OnException()
{
  if (m_invoker == NULL)
    return;

  m_invoker->onExecutionFailed();
  m_invocationManager->OnExecutionDone(GetId());
}

bool CLanguageInvokerThread::GetCleanupIds(std::vector<int>& ids)
{
  if (!m_cleanup_params)
    return false;

  time_t cur = time(NULL);
  time_t old = m_last_check.exchange(cur);
  if (cur == old)
    return false;
  std::unique_lock<std::mutex> lck(m_mutex);
  return m_cleanup_params->GetCleanupIDs(cur, ids);
}

void CLanguageInvokerThread::Cleanup(const std::vector<int>& ids)
{
  if (!m_reusable || !m_invoker || !m_cleanup_params || !CThread::IsRunning() || ids.empty())
    return;

  CLog::Log(LOGDEBUG, "CLanguageInvokerThread(%d, %s): performing plugin cleanup", m_invoker ? m_invoker->GetId() : GetId(), m_script.c_str());

  std::unique_lock<std::mutex> lck(m_mutex);
  m_invoker->Reset();
  m_cleanup_ids = ids;
  m_restart = true;
  m_condition.notify_one();
}

void CLanguageInvokerThread::CleanupAtExit()
{
  CLog::Log(LOGDEBUG, "CLanguageInvokerThread(%d, %s): performing plugin cleanup at exit", m_invoker ? m_invoker->GetId() : GetId(), m_script.c_str());

  Release();
  StopThread(false);

  XbmcThreads::EndTime timeout(1000);
  while (WaitForThreadExit(10))
    if (timeout.IsTimePast())
    {
      CLog::Log(LOGERROR, "CLanguageInvokerThread(%d, %s): plugin cleanup didn't complete in 1 second", m_invoker ? m_invoker->GetId() : GetId(), m_script.c_str());
      break;
    }
}
