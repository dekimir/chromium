// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_selector.h"

#include <utility>

#include "base/logging.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/traced_value.h"

namespace base {
namespace sequence_manager {
namespace internal {

TaskQueueSelector::TaskQueueSelector(
    scoped_refptr<AssociatedThreadId> associated_thread)
    : associated_thread_(std::move(associated_thread)),
      delayed_work_queue_sets_(TaskQueue::kQueuePriorityCount, "delayed"),
      immediate_work_queue_sets_(TaskQueue::kQueuePriorityCount, "immediate") {}

TaskQueueSelector::~TaskQueueSelector() = default;

void TaskQueueSelector::AddQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  AddQueueImpl(queue, TaskQueue::kNormalPriority);
}

void TaskQueueSelector::RemoveQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (queue->IsQueueEnabled()) {
    RemoveQueueImpl(queue);
  }
}

void TaskQueueSelector::EnableQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  AddQueueImpl(queue, queue->GetQueuePriority());
  if (task_queue_selector_observer_)
    task_queue_selector_observer_->OnTaskQueueEnabled(queue);
}

void TaskQueueSelector::DisableQueue(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(!queue->IsQueueEnabled());
  RemoveQueueImpl(queue);
}

void TaskQueueSelector::SetQueuePriority(internal::TaskQueueImpl* queue,
                                         TaskQueue::QueuePriority priority) {
  DCHECK_LT(priority, TaskQueue::kQueuePriorityCount);
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (queue->IsQueueEnabled()) {
    ChangeSetIndex(queue, priority);
  } else {
    // Disabled queue is not in any set so we can't use ChangeSetIndex here
    // and have to assign priority for the queue itself.
    queue->delayed_work_queue()->AssignSetIndex(priority);
    queue->immediate_work_queue()->AssignSetIndex(priority);
  }
  DCHECK_EQ(priority, queue->GetQueuePriority());
}

TaskQueue::QueuePriority TaskQueueSelector::NextPriority(
    TaskQueue::QueuePriority priority) {
  DCHECK(priority < TaskQueue::kQueuePriorityCount);
  return static_cast<TaskQueue::QueuePriority>(static_cast<int>(priority) + 1);
}

void TaskQueueSelector::AddQueueImpl(internal::TaskQueueImpl* queue,
                                     TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.AddQueue(queue->delayed_work_queue(), priority);
  immediate_work_queue_sets_.AddQueue(queue->immediate_work_queue(), priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::ChangeSetIndex(internal::TaskQueueImpl* queue,
                                       TaskQueue::QueuePriority priority) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.ChangeSetIndex(queue->delayed_work_queue(),
                                          priority);
  immediate_work_queue_sets_.ChangeSetIndex(queue->immediate_work_queue(),
                                            priority);
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
}

void TaskQueueSelector::RemoveQueueImpl(internal::TaskQueueImpl* queue) {
#if DCHECK_IS_ON()
  DCHECK(CheckContainsQueueForTest(queue));
#endif
  delayed_work_queue_sets_.RemoveQueue(queue->delayed_work_queue());
  immediate_work_queue_sets_.RemoveQueue(queue->immediate_work_queue());

#if DCHECK_IS_ON()
  DCHECK(!CheckContainsQueueForTest(queue));
#endif
}

WorkQueue* TaskQueueSelector::ChooseOldestImmediateTaskWithPriority(
    TaskQueue::QueuePriority priority) const {
  return immediate_work_queue_sets_.GetOldestQueueInSet(priority);
}

WorkQueue* TaskQueueSelector::ChooseOldestDelayedTaskWithPriority(
    TaskQueue::QueuePriority priority) const {
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority);
}

WorkQueue* TaskQueueSelector::ChooseOldestImmediateOrDelayedTaskWithPriority(
    TaskQueue::QueuePriority priority,
    bool* out_chose_delayed_over_immediate) const {
  DCHECK_EQ(*out_chose_delayed_over_immediate, false);
  EnqueueOrder immediate_enqueue_order;
  WorkQueue* immediate_queue =
      immediate_work_queue_sets_.GetOldestQueueAndEnqueueOrderInSet(
          priority, &immediate_enqueue_order);
  if (immediate_queue) {
    EnqueueOrder delayed_enqueue_order;
    WorkQueue* delayed_queue =
        delayed_work_queue_sets_.GetOldestQueueAndEnqueueOrderInSet(
            priority, &delayed_enqueue_order);
    if (!delayed_queue)
      return immediate_queue;

    if (immediate_enqueue_order < delayed_enqueue_order) {
      return immediate_queue;
    } else {
      *out_chose_delayed_over_immediate = true;
      return delayed_queue;
    }
  }
  return delayed_work_queue_sets_.GetOldestQueueInSet(priority);
}

WorkQueue* TaskQueueSelector::ChooseOldestWithPriority(
    TaskQueue::QueuePriority priority,
    bool* out_chose_delayed_over_immediate) const {
  // Select an immediate work queue if we are starving immediate tasks.
  if (immediate_starvation_count_ >= kMaxDelayedStarvationTasks) {
    WorkQueue* queue = ChooseOldestImmediateTaskWithPriority(priority);
    if (queue)
      return queue;
    return ChooseOldestDelayedTaskWithPriority(priority);
  }
  return ChooseOldestImmediateOrDelayedTaskWithPriority(
      priority, out_chose_delayed_over_immediate);
}

WorkQueue* TaskQueueSelector::SelectWorkQueueToServiceImpl(
    TaskQueue::QueuePriority max_priority,
    bool* out_chose_delayed_over_immediate) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK_EQ(*out_chose_delayed_over_immediate, false);

  // Always service the control queue if it has any work.
  if (max_priority > TaskQueue::kControlPriority) {
    WorkQueue* queue = ChooseOldestWithPriority(
        TaskQueue::kControlPriority, out_chose_delayed_over_immediate);
    if (queue)
      return queue;
  }

  // Select from the low priority queue if we are starving it.
  if (max_priority > TaskQueue::kLowPriority &&
      low_priority_starvation_score_ >= kMaxLowPriorityStarvationScore) {
    WorkQueue* queue = ChooseOldestWithPriority(
        TaskQueue::kLowPriority, out_chose_delayed_over_immediate);
    if (queue)
      return queue;
  }

  // Select from the normal priority queue if we are starving it.
  if (max_priority > TaskQueue::kNormalPriority &&
      normal_priority_starvation_score_ >= kMaxNormalPriorityStarvationScore) {
    WorkQueue* queue = ChooseOldestWithPriority(
        TaskQueue::kNormalPriority, out_chose_delayed_over_immediate);
    if (queue)
      return queue;
  }

  // Select from the high priority queue if we are starving it.
  if (max_priority > TaskQueue::kHighPriority &&
      high_priority_starvation_score_ >= kMaxHighPriorityStarvationScore) {
    WorkQueue* queue = ChooseOldestWithPriority(
        TaskQueue::kHighPriority, out_chose_delayed_over_immediate);
    if (queue)
      return queue;
  }

  // Otherwise choose in priority order.
  for (TaskQueue::QueuePriority priority = TaskQueue::kHighestPriority;
       priority < max_priority; priority = NextPriority(priority)) {
    WorkQueue* queue =
        ChooseOldestWithPriority(priority, out_chose_delayed_over_immediate);
    if (queue)
      return queue;
  }
  return nullptr;
}

#if DCHECK_IS_ON() || !defined(NDEBUG)
bool TaskQueueSelector::CheckContainsQueueForTest(
    const internal::TaskQueueImpl* queue) const {
  bool contains_delayed_work_queue =
      delayed_work_queue_sets_.ContainsWorkQueueForTest(
          queue->delayed_work_queue());

  bool contains_immediate_work_queue =
      immediate_work_queue_sets_.ContainsWorkQueueForTest(
          queue->immediate_work_queue());

  DCHECK_EQ(contains_delayed_work_queue, contains_immediate_work_queue);
  return contains_delayed_work_queue;
}
#endif

WorkQueue* TaskQueueSelector::SelectWorkQueueToService() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  bool chose_delayed_over_immediate = false;
  WorkQueue* queue = SelectWorkQueueToServiceImpl(
      TaskQueue::kQueuePriorityCount, &chose_delayed_over_immediate);
  if (queue) {
    // We could use |(*out_work_queue)->task_queue()->GetQueuePriority()| here
    // but for re-queued non-nestable tasks |task_queue()| returns null.
    DidSelectQueueWithPriority(
        static_cast<TaskQueue::QueuePriority>(queue->work_queue_set_index()),
        chose_delayed_over_immediate);
  }
  return queue;
}

void TaskQueueSelector::DidSelectQueueWithPriority(
    TaskQueue::QueuePriority priority,
    bool chose_delayed_over_immediate) {
  switch (priority) {
    case TaskQueue::kControlPriority:
      break;
    case TaskQueue::kHighestPriority:
      low_priority_starvation_score_ +=
          HasTasksWithPriority(TaskQueue::kLowPriority)
              ? kSmallScoreIncrementForLowPriorityStarvation
              : 0;
      normal_priority_starvation_score_ +=
          HasTasksWithPriority(TaskQueue::kNormalPriority)
              ? kSmallScoreIncrementForNormalPriorityStarvation
              : 0;
      high_priority_starvation_score_ +=
          HasTasksWithPriority(TaskQueue::kHighPriority)
              ? kSmallScoreIncrementForHighPriorityStarvation
              : 0;
      break;
    case TaskQueue::kHighPriority:
      low_priority_starvation_score_ +=
          HasTasksWithPriority(TaskQueue::kLowPriority)
              ? kLargeScoreIncrementForLowPriorityStarvation
              : 0;
      normal_priority_starvation_score_ +=
          HasTasksWithPriority(TaskQueue::kNormalPriority)
              ? kLargeScoreIncrementForNormalPriorityStarvation
              : 0;
      high_priority_starvation_score_ = 0;
      break;
    case TaskQueue::kNormalPriority:
      low_priority_starvation_score_ +=
          HasTasksWithPriority(TaskQueue::kLowPriority)
              ? kLargeScoreIncrementForLowPriorityStarvation
              : 0;
      normal_priority_starvation_score_ = 0;
      break;
    case TaskQueue::kLowPriority:
    case TaskQueue::kBestEffortPriority:
      low_priority_starvation_score_ = 0;
      high_priority_starvation_score_ = 0;
      normal_priority_starvation_score_ = 0;
      break;
    default:
      NOTREACHED();
  }
  if (chose_delayed_over_immediate) {
    immediate_starvation_count_++;
  } else {
    immediate_starvation_count_ = 0;
  }
}

void TaskQueueSelector::AsValueInto(trace_event::TracedValue* state) const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  state->SetInteger("high_priority_starvation_score",
                    high_priority_starvation_score_);
  state->SetInteger("normal_priority_starvation_score",
                    normal_priority_starvation_score_);
  state->SetInteger("low_priority_starvation_score",
                    low_priority_starvation_score_);
  state->SetInteger("immediate_starvation_count", immediate_starvation_count_);
}

void TaskQueueSelector::SetTaskQueueSelectorObserver(Observer* observer) {
  task_queue_selector_observer_ = observer;
}

bool TaskQueueSelector::AllEnabledWorkQueuesAreEmpty() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  for (TaskQueue::QueuePriority priority = TaskQueue::kControlPriority;
       priority < TaskQueue::kQueuePriorityCount;
       priority = NextPriority(priority)) {
    if (!delayed_work_queue_sets_.IsSetEmpty(priority) ||
        !immediate_work_queue_sets_.IsSetEmpty(priority)) {
      return false;
    }
  }
  return true;
}

void TaskQueueSelector::SetImmediateStarvationCountForTest(
    size_t immediate_starvation_count) {
  immediate_starvation_count_ = immediate_starvation_count;
}

bool TaskQueueSelector::HasTasksWithPriority(
    TaskQueue::QueuePriority priority) {
  return !delayed_work_queue_sets_.IsSetEmpty(priority) ||
         !immediate_work_queue_sets_.IsSetEmpty(priority);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
