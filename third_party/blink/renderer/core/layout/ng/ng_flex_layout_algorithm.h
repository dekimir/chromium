// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBreakToken;
class NGConstraintSpace;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGFlexLayoutAlgorithm(NGBlockNode,
                        const NGConstraintSpace&,
                        const NGBreakToken*);

  scoped_refptr<NGLayoutResult> Layout() override;

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;

 private:
  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(NGBlockNode child);
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size);

  void HandleOutOfFlowPositioned(NGBlockNode child);

  NGLogicalSize border_box_size_;
  NGBoxStrut border_scrollbar_padding_;
  NGLogicalSize content_box_size_;
  NGBoxStrut borders_;
  NGBoxStrut padding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_FLEX_LAYOUT_ALGORITHM_H_
