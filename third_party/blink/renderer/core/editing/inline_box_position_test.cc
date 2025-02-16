// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/inline_box_position.h"

#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

std::ostream& operator<<(std::ostream& ostream,
                         const InlineBoxPosition& inline_box_position) {
  if (!inline_box_position.inline_box)
    return ostream << "null";
  return ostream
         << inline_box_position.inline_box->GetLineLayoutItem().GetNode() << "@"
         << inline_box_position.offset_in_box;
}

class InlineBoxPositionTest : public EditingTestBase {};

TEST_F(InlineBoxPositionTest, ComputeInlineBoxPositionBidiIsolate) {
  // InlineBoxPosition is a legacy-only data structure.
  ScopedLayoutNGForTest scoped_layout_ng(false);

  // "|" is bidi-level 0, and "foo" and "bar" are bidi-level 2
  SetBodyContent(
      "|<span id=sample style='unicode-bidi: isolate;'>foo<br>bar</span>|");

  Element* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const InlineBoxPosition& actual =
      ComputeInlineBoxPosition(PositionWithAffinity(Position(text, 0)));
  EXPECT_EQ(ToLayoutText(text->GetLayoutObject())->FirstTextBox(),
            actual.inline_box);
}

// http://crbug.com/716093
TEST_F(InlineBoxPositionTest, ComputeInlineBoxPositionMixedEditable) {
  // InlineBoxPosition is a legacy-only data structure.
  ScopedLayoutNGForTest scoped_layout_ng(false);

  SetBodyContent(
      "<div contenteditable id=sample>abc<input contenteditable=false></div>");
  Element* const sample = GetDocument().getElementById("sample");

  const InlineBoxPosition& actual = ComputeInlineBoxPosition(
      PositionWithAffinity(Position::LastPositionInNode(*sample)));
  // Should not be in infinite-loop
  EXPECT_EQ(nullptr, actual.inline_box);
  EXPECT_EQ(0, actual.offset_in_box);
}

// http://crbug.com/841363
TEST_F(InlineBoxPositionTest, InFlatTreeAfterInputWithPlaceholderDoesntCrash) {
  // InlineBoxPosition is a legacy-only data structure.
  ScopedLayoutNGForTest scoped_layout_ng(false);

  SetBodyContent("foo <input placeholder=bla> bar");
  const Element* const input = GetDocument().QuerySelector("input");
  const LayoutBox* const input_layout = ToLayoutBox(input->GetLayoutObject());
  const InlineBox* const input_wrapper = input_layout->InlineBoxWrapper();
  const PositionInFlatTreeWithAffinity after_input(
      PositionInFlatTree::AfterNode(*input));

  // Should't crash inside
  const InlineBoxPosition box_position = ComputeInlineBoxPosition(after_input);
  EXPECT_EQ(input_wrapper, box_position.inline_box);
  EXPECT_EQ(1, box_position.offset_in_box);
}

}  // namespace blink
