// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/activation_state_computing_navigation_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

// static
std::unique_ptr<ActivationStateComputingNavigationThrottle>
ActivationStateComputingNavigationThrottle::CreateForMainFrame(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle->IsInMainFrame());
  return base::WrapUnique(new ActivationStateComputingNavigationThrottle(
      navigation_handle, base::Optional<ActivationState>(), nullptr));
}

// static
std::unique_ptr<ActivationStateComputingNavigationThrottle>
ActivationStateComputingNavigationThrottle::CreateForSubframe(
    content::NavigationHandle* navigation_handle,
    VerifiedRuleset::Handle* ruleset_handle,
    const ActivationState& parent_activation_state) {
  DCHECK(!navigation_handle->IsInMainFrame());
  DCHECK_NE(ActivationLevel::DISABLED,
            parent_activation_state.activation_level);
  DCHECK(ruleset_handle);
  return base::WrapUnique(new ActivationStateComputingNavigationThrottle(
      navigation_handle, parent_activation_state, ruleset_handle));
}

ActivationStateComputingNavigationThrottle::
    ActivationStateComputingNavigationThrottle(
        content::NavigationHandle* navigation_handle,
        const base::Optional<ActivationState> parent_activation_state,
        VerifiedRuleset::Handle* ruleset_handle)
    : content::NavigationThrottle(navigation_handle),
      parent_activation_state_(parent_activation_state),
      ruleset_handle_(ruleset_handle),
      weak_ptr_factory_(this) {}

ActivationStateComputingNavigationThrottle::
    ~ActivationStateComputingNavigationThrottle() {
  if (!destruction_closure_.is_null())
    std::move(destruction_closure_).Run();
}

void ActivationStateComputingNavigationThrottle::
    NotifyPageActivationWithRuleset(
        VerifiedRuleset::Handle* ruleset_handle,
        const ActivationState& page_activation_state) {
  DCHECK(navigation_handle()->IsInMainFrame());
  DCHECK_NE(ActivationLevel::DISABLED, page_activation_state.activation_level);
  parent_activation_state_ = page_activation_state;
  ruleset_handle_ = ruleset_handle;
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillStartRequest() {
  if (parent_activation_state_)
    CheckActivationState();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillRedirectRequest() {
  if (parent_activation_state_)
    CheckActivationState();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillProcessResponse() {
  // If no parent activation, this is main frame that was never notified of
  // activation.
  if (!parent_activation_state_) {
    DCHECK(navigation_handle()->IsInMainFrame());
    DCHECK(!async_filter_);
    DCHECK(!ruleset_handle_);
    return content::NavigationThrottle::PROCEED;
  }

  // Throttles which have finished their last check should just proceed here.
  // All others need to defer and either wait for their existing check to
  // finish, or start a new check now if there was no previous speculative
  // check.
  if (async_filter_ && async_filter_->has_activation_state()) {
    LogDelayMetrics(base::TimeDelta::FromMilliseconds(0));
    if (navigation_handle()->IsInMainFrame())
      UpdateWithMoreAccurateState();
    return content::NavigationThrottle::PROCEED;
  }
  DCHECK(!defer_timer_);
  defer_timer_ = std::make_unique<base::ElapsedTimer>();
  if (!async_filter_) {
    DCHECK(navigation_handle()->IsInMainFrame());
    CheckActivationState();
  }
  return content::NavigationThrottle::DEFER;
}

const char* ActivationStateComputingNavigationThrottle::GetNameForLogging() {
  return "ActivationStateComputingNavigationThrottle";
}

void ActivationStateComputingNavigationThrottle::CheckActivationState() {
  DCHECK(parent_activation_state_);
  DCHECK(ruleset_handle_);
  AsyncDocumentSubresourceFilter::InitializationParams params;
  params.document_url = navigation_handle()->GetURL();
  params.parent_activation_state = parent_activation_state_.value();
  if (!navigation_handle()->IsInMainFrame()) {
    content::RenderFrameHost* parent = navigation_handle()->GetParentFrame();
    DCHECK(parent);
    params.parent_document_origin = parent->GetLastCommittedOrigin();
  }

  // If there is an existing |async_filter_| that hasn't called
  // OnActivationStateComputed, it will be deleted here and never call that
  // method. This is by design of the AsyncDocumentSubresourceFilter, which
  // will drop the message via weak pointer semantics.
  async_filter_ = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle_, std::move(params),
      base::Bind(&ActivationStateComputingNavigationThrottle::
                     OnActivationStateComputed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ActivationStateComputingNavigationThrottle::OnActivationStateComputed(
    ActivationState state) {
  if (defer_timer_) {
    LogDelayMetrics(defer_timer_->Elapsed());
    if (navigation_handle()->IsInMainFrame())
      UpdateWithMoreAccurateState();
    Resume();
  }
}

void ActivationStateComputingNavigationThrottle::UpdateWithMoreAccurateState() {
  // This method is only needed for main frame navigations that are notified of
  // page activation more than once. Even for those that are updated once, it
  // should be a no-op.
  DCHECK(navigation_handle()->IsInMainFrame());
  DCHECK(parent_activation_state_);
  DCHECK(async_filter_);
  async_filter_->UpdateWithMoreAccurateState(*parent_activation_state_);
}

void ActivationStateComputingNavigationThrottle::LogDelayMetrics(
    base::TimeDelta delay) const {
  UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
      "SubresourceFilter.DocumentLoad.ActivationComputingDelay", delay,
      base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(10),
      50);
  if (navigation_handle()->IsInMainFrame()) {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "SubresourceFilter.DocumentLoad.ActivationComputingDelay.MainFrame",
        delay, base::TimeDelta::FromMicroseconds(1),
        base::TimeDelta::FromSeconds(10), 50);
  }
}

AsyncDocumentSubresourceFilter*
ActivationStateComputingNavigationThrottle::filter() const {
  // TODO(csharrison): This should not really be necessary, as we should be
  // delaying the navigation until the filter has computed an activation state.
  // See crbug.com/736249. In the mean time, have a check here to avoid
  // returning a filter in an invalid state.
  if (async_filter_ && async_filter_->has_activation_state())
    return async_filter_.get();
  return nullptr;
}

// Ensure the caller cannot take ownership of a subresource filter for cases
// when activation IPCs are not sent to the render process.
std::unique_ptr<AsyncDocumentSubresourceFilter>
ActivationStateComputingNavigationThrottle::ReleaseFilter() {
  return will_send_activation_to_renderer_ ? std::move(async_filter_) : nullptr;
}

void ActivationStateComputingNavigationThrottle::
    WillSendActivationToRenderer() {
  DCHECK(async_filter_);
  will_send_activation_to_renderer_ = true;
}

}  // namespace subresource_filter
