// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/compute_attributes.h"

#include <cstddef>

#include "base/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {
namespace {

base::Optional<int32_t> GetCellAttribute(
    const ui::AXPlatformNodeDelegate* delegate,
    ax::mojom::IntAttribute attribute) {
  switch (attribute) {
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
      return delegate->GetTableCellAriaColIndex();
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
      return delegate->GetTableCellAriaRowIndex();
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
      return delegate->GetTableCellColIndex();
    case ax::mojom::IntAttribute::kTableCellRowIndex:
      return delegate->GetTableCellRowIndex();
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
      return delegate->GetTableCellColSpan();
    case ax::mojom::IntAttribute::kTableCellRowSpan:
      return delegate->GetTableCellRowSpan();
    default:
      return base::nullopt;
  }
}

base::Optional<int32_t> GetRowAttribute(
    const ui::AXPlatformNodeDelegate* delegate,
    ax::mojom::IntAttribute attribute) {
  if (attribute == ax::mojom::IntAttribute::kTableRowIndex) {
    return delegate->GetTableRowRowIndex();
  }
  return base::nullopt;
}

base::Optional<int32_t> GetTableAttribute(
    const ui::AXPlatformNodeDelegate* delegate,
    ax::mojom::IntAttribute attribute) {
  switch (attribute) {
    case ax::mojom::IntAttribute::kTableColumnCount:
      return delegate->GetTableColCount();
    case ax::mojom::IntAttribute::kTableRowCount:
      return delegate->GetTableRowCount();
    case ax::mojom::IntAttribute::kAriaColumnCount:
      return delegate->GetTableAriaColCount();
    case ax::mojom::IntAttribute::kAriaRowCount:
      return delegate->GetTableAriaRowCount();
    default:
      return base::nullopt;
  }
}

base::Optional<int32_t> GetFromData(const ui::AXPlatformNodeDelegate* delegate,
                                    ax::mojom::IntAttribute attribute) {
  int32_t value;
  if (delegate->GetData().GetIntAttribute(attribute, &value)) {
    return value;
  }
  return base::nullopt;
}

}  // namespace

base::Optional<int32_t> ComputeAttribute(
    const ui::AXPlatformNodeDelegate* delegate,
    ax::mojom::IntAttribute attribute) {
  base::Optional<int32_t> maybe_value = base::nullopt;
  if (delegate->IsTableCellOrHeader())
    maybe_value = GetCellAttribute(delegate, attribute);
  else if (delegate->IsTableRow())
    maybe_value = GetRowAttribute(delegate, attribute);
  else if (delegate->IsTable())
    maybe_value = GetTableAttribute(delegate, attribute);

  if (!maybe_value.has_value()) {
    return GetFromData(delegate, attribute);
  }
  return maybe_value;
}

}  // namespace ui
