// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_language_info.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/transform.h"

namespace ui {

AXNode::AXNode(AXNode::OwnerTree* tree,
               AXNode* parent,
               int32_t id,
               int32_t index_in_parent)
    : tree_(tree),
      index_in_parent_(index_in_parent),
      parent_(parent),
      language_info_(nullptr) {
  data_.id = id;
}

AXNode::~AXNode() = default;

int AXNode::GetUnignoredChildCount() const {
  int count = 0;
  for (int i = 0; i < child_count(); i++) {
    AXNode* child = children_[i];
    if (child->data().HasState(ax::mojom::State::kIgnored))
      count += child->GetUnignoredChildCount();
    else
      count++;
  }
  return count;
}

AXNode* AXNode::GetUnignoredChildAtIndex(int index) const {
  int count = 0;
  for (int i = 0; i < child_count(); i++) {
    AXNode* child = children_[i];
    if (child->data().HasState(ax::mojom::State::kIgnored)) {
      int nested_child_count = child->GetUnignoredChildCount();
      if (index < count + nested_child_count)
        return child->GetUnignoredChildAtIndex(index - count);
      else
        count += nested_child_count;
    } else {
      if (count == index)
        return child;
      else
        count++;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetUnignoredParent() const {
  AXNode* result = parent();
  while (result && result->data().HasState(ax::mojom::State::kIgnored))
    result = result->parent();
  return result;
}

int AXNode::GetUnignoredIndexInParent() const {
  AXNode* parent = GetUnignoredParent();
  if (parent) {
    for (int i = 0; i < parent->GetUnignoredChildCount(); ++i) {
      if (parent->GetUnignoredChildAtIndex(i) == this)
        return i;
    }
  }

  return 0;
}

bool AXNode::IsTextNode() const {
  return data().role == ax::mojom::Role::kStaticText ||
         data().role == ax::mojom::Role::kLineBreak ||
         data().role == ax::mojom::Role::kInlineTextBox;
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(int32_t offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.relative_bounds.offset_container_id = offset_container_id;
  data_.relative_bounds.bounds = location;
  if (transform)
    data_.relative_bounds.transform.reset(new gfx::Transform(*transform));
  else
    data_.relative_bounds.transform.reset(nullptr);
}

void AXNode::SetIndexInParent(int index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::SwapChildren(std::vector<AXNode*>& children) {
  children.swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(AXNode* ancestor) {
  if (this == ancestor)
    return true;
  else if (parent())
    return parent()->IsDescendantOf(ancestor);

  return false;
}

std::vector<int> AXNode::GetOrComputeLineStartOffsets() {
  std::vector<int> line_offsets;
  if (data().GetIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                                 &line_offsets))
    return line_offsets;

  int start_offset = 0;
  ComputeLineStartOffsets(&line_offsets, &start_offset);
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                            line_offsets);
  return line_offsets;
}

void AXNode::ComputeLineStartOffsets(std::vector<int>* line_offsets,
                                     int* start_offset) const {
  DCHECK(line_offsets);
  DCHECK(start_offset);
  for (const AXNode* child : children()) {
    DCHECK(child);
    if (child->child_count()) {
      child->ComputeLineStartOffsets(line_offsets, start_offset);
      continue;
    }

    // Don't report if the first piece of text starts a new line or not.
    if (*start_offset && !child->data().HasIntAttribute(
                             ax::mojom::IntAttribute::kPreviousOnLineId)) {
      // If there are multiple objects with an empty accessible label at the
      // start of a line, only include a single line start offset.
      if (line_offsets->empty() || line_offsets->back() != *start_offset)
        line_offsets->push_back(*start_offset);
    }

    base::string16 text =
        child->data().GetString16Attribute(ax::mojom::StringAttribute::kName);
    *start_offset += static_cast<int>(text.length());
  }
}

const std::string& AXNode::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXNode* current_node = this;
  do {
    if (current_node->data().HasStringAttribute(attribute))
      return current_node->data().GetStringAttribute(attribute);
    current_node = current_node->parent();
  } while (current_node);
  return base::EmptyString();
}

base::string16 AXNode::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

const AXLanguageInfo* AXNode::GetLanguageInfo() {
  if (language_info_)
    return language_info_.get();

  const auto& lang_attr =
      GetStringAttribute(ax::mojom::StringAttribute::kLanguage);

  // Promote language attribute to LanguageInfo.
  if (!lang_attr.empty()) {
    language_info_.reset(new AXLanguageInfo(this, lang_attr));
    return language_info_.get();
  }

  // Try search for language through parent instead.
  if (!parent())
    return nullptr;

  const AXLanguageInfo* parent_lang_info = parent()->GetLanguageInfo();
  if (!parent_lang_info)
    return nullptr;

  // Cache the results on this node.
  language_info_.reset(new AXLanguageInfo(parent_lang_info, this));
  return language_info_.get();
}

std::string AXNode::GetLanguage() {
  const AXLanguageInfo* lang_info = GetLanguageInfo();

  if (lang_info)
    return lang_info->language();

  return "";
}

std::ostream& operator<<(std::ostream& stream, const AXNode& node) {
  return stream << node.data().ToString();
}

bool AXNode::IsTable() const {
  return IsTableLike(data().role);
}

int32_t AXNode::GetTableColCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return table_info->col_count;
}

int32_t AXNode::GetTableRowCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return table_info->row_count;
}

int32_t AXNode::GetTableAriaColCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return table_info->aria_col_count;
}

int32_t AXNode::GetTableAriaRowCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return table_info->aria_row_count;
}

int32_t AXNode::GetTableCellCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return static_cast<int32_t>(table_info->unique_cell_ids.size());
}

AXNode* AXNode::GetTableCellFromIndex(int32_t index) const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  if (index < 0 ||
      index >= static_cast<int32_t>(table_info->unique_cell_ids.size()))
    return nullptr;

  return tree_->GetFromId(table_info->unique_cell_ids[index]);
}

AXNode* AXNode::GetTableCellFromCoords(int32_t row_index,
                                       int32_t col_index) const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  if (row_index < 0 || row_index >= table_info->row_count || col_index < 0 ||
      col_index >= table_info->col_count)
    return nullptr;

  return tree_->GetFromId(table_info->cell_ids[row_index][col_index]);
}

void AXNode::GetTableColHeaderNodeIds(
    int32_t col_index,
    std::vector<int32_t>* col_header_ids) const {
  DCHECK(col_header_ids);
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return;

  if (col_index < 0 || col_index >= table_info->col_count)
    return;

  for (size_t i = 0; i < table_info->col_headers[col_index].size(); i++)
    col_header_ids->push_back(table_info->col_headers[col_index][i]);
}

void AXNode::GetTableRowHeaderNodeIds(
    int32_t row_index,
    std::vector<int32_t>* row_header_ids) const {
  DCHECK(row_header_ids);
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return;

  if (row_index < 0 || row_index >= table_info->row_count)
    return;

  for (size_t i = 0; i < table_info->row_headers[row_index].size(); i++)
    row_header_ids->push_back(table_info->row_headers[row_index][i]);
}

void AXNode::GetTableUniqueCellIds(std::vector<int32_t>* cell_ids) const {
  DCHECK(cell_ids);
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return;

  cell_ids->assign(table_info->unique_cell_ids.begin(),
                   table_info->unique_cell_ids.end());
}

std::vector<AXNode*>* AXNode::GetExtraMacNodes() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  return &table_info->extra_mac_nodes;
}

//
// Table row-like nodes.
//

bool AXNode::IsTableRow() const {
  return data().role == ax::mojom::Role::kRow;
}

int32_t AXNode::GetTableRowRowIndex() const {
  // TODO(dmazzoni): Compute from AXTableInfo. http://crbug.com/832289
  int32_t row_index = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, &row_index);
  return row_index;
}

//
// Table cell-like nodes.
//

bool AXNode::IsTableCellOrHeader() const {
  return IsCellOrTableHeader(data().role);
}

int32_t AXNode::GetTableCellIndex() const {
  if (!IsTableCellOrHeader())
    return -1;

  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return -1;

  const auto& iter = table_info->cell_id_to_index.find(id());
  if (iter != table_info->cell_id_to_index.end())
    return iter->second;

  return -1;
}

int32_t AXNode::GetTableCellColIndex() const {
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return 0;

  int32_t index = GetTableCellIndex();
  if (index == -1)
    return 0;

  return table_info->cell_data_vector[index].col_index;
}

int32_t AXNode::GetTableCellRowIndex() const {
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return 0;

  int32_t index = GetTableCellIndex();
  if (index == -1)
    return 0;

  return table_info->cell_data_vector[index].row_index;
}

int32_t AXNode::GetTableCellColSpan() const {
  // If it's not a table cell, don't return a col span.
  if (!IsTableCellOrHeader())
    return 0;

  // Otherwise, try to return a colspan, with 1 as the default if it's not
  // specified.
  int32_t col_span = 1;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan, &col_span))
    return col_span;

  return 1;
}

int32_t AXNode::GetTableCellRowSpan() const {
  // If it's not a table cell, don't return a row span.
  if (!IsTableCellOrHeader())
    return 0;

  // Otherwise, try to return a row span, with 1 as the default if it's not
  // specified.
  int32_t row_span = 1;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, &row_span))
    return row_span;
  return 1;
}

int32_t AXNode::GetTableCellAriaColIndex() const {
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return -0;

  int32_t index = GetTableCellIndex();
  if (index == -1)
    return 0;

  return table_info->cell_data_vector[index].aria_col_index;
}

int32_t AXNode::GetTableCellAriaRowIndex() const {
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return -0;

  int32_t index = GetTableCellIndex();
  if (index == -1)
    return 0;

  return table_info->cell_data_vector[index].aria_row_index;
}

void AXNode::GetTableCellColHeaderNodeIds(
    std::vector<int32_t>* col_header_ids) const {
  DCHECK(col_header_ids);
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  int32_t col_index = GetTableCellColIndex();
  if (col_index < 0 || col_index >= table_info->col_count)
    return;

  for (size_t i = 0; i < table_info->col_headers[col_index].size(); i++)
    col_header_ids->push_back(table_info->col_headers[col_index][i]);
}

void AXNode::GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const {
  DCHECK(col_headers);

  std::vector<int32_t> col_header_ids;
  GetTableCellColHeaderNodeIds(&col_header_ids);
  IdVectorToNodeVector(col_header_ids, col_headers);
}

void AXNode::GetTableCellRowHeaderNodeIds(
    std::vector<int32_t>* row_header_ids) const {
  DCHECK(row_header_ids);
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  int32_t row_index = GetTableCellRowIndex();
  if (row_index < 0 || row_index >= table_info->row_count)
    return;

  for (size_t i = 0; i < table_info->row_headers[row_index].size(); i++)
    row_header_ids->push_back(table_info->row_headers[row_index][i]);
}

void AXNode::GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const {
  DCHECK(row_headers);

  std::vector<int32_t> row_header_ids;
  GetTableCellRowHeaderNodeIds(&row_header_ids);
  IdVectorToNodeVector(row_header_ids, row_headers);
}

AXTableInfo* AXNode::GetAncestorTableInfo() const {
  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->parent();
  if (node)
    return tree_->GetTableInfo(node);
  return nullptr;
}

void AXNode::IdVectorToNodeVector(std::vector<int32_t>& ids,
                                  std::vector<AXNode*>* nodes) const {
  for (int32_t id : ids) {
    AXNode* node = tree_->GetFromId(id);
    if (node)
      nodes->push_back(node);
  }
}

// pos_in_set and set_size related functions.
// Uses AXTree's cache to calculate node's pos_in_set.
int32_t AXNode::GetPosInSet() {
  // Only allow this to be called on nodes that can hold pos_in_set values,
  // which are defined in the ARIA spec.
  if (!IsItemLike(data().role)) {
    return 0;
  }

  const AXNode* ordered_set = GetOrderedSet();
  if (!ordered_set) {
    return 0;
  }

  // See AXTree::GetPosInSet
  return tree_->GetPosInSet(*this, ordered_set);
}

// Uses AXTree's cache to calculate node's set_size.
int32_t AXNode::GetSetSize() {
  // Only allow this to be called on nodes that can hold set_size values, which
  // are defined in the ARIA spec.
  if (!(IsItemLike(data().role) || IsSetLike(data().role)))
    return 0;

  // If node is item-like, find its outerlying ordered set. Otherwise,
  // this node is the ordered set.
  const AXNode* ordered_set = this;
  if (IsItemLike(data().role))
    ordered_set = GetOrderedSet();
  if (!ordered_set)
    return 0;

  // See AXTree::GetSetSize
  return tree_->GetSetSize(*this, ordered_set);
}

// Returns true if the role of ordered set matches the role of item.
// Returns false otherwise.
bool AXNode::SetRoleMatchesItemRole(const AXNode* ordered_set) const {
  ax::mojom::Role item_role = data().role;

  // Switch on role of ordered set
  switch (ordered_set->data().role) {
    case ax::mojom::Role::kFeed:
      return item_role == ax::mojom::Role::kArticle;

    case ax::mojom::Role::kList:
      return item_role == ax::mojom::Role::kListItem;

    case ax::mojom::Role::kGroup:
      return item_role == ax::mojom::Role::kListItem ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kTreeItem;

    case ax::mojom::Role::kMenu:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;

    case ax::mojom::Role::kMenuBar:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;

    case ax::mojom::Role::kTabList:
      return item_role == ax::mojom::Role::kTab;

    case ax::mojom::Role::kTree:
      return item_role == ax::mojom::Role::kTreeItem;

    case ax::mojom::Role::kListBox:
      return item_role == ax::mojom::Role::kListBoxOption;

    case ax::mojom::Role::kMenuListPopup:
      return item_role == ax::mojom::Role::kMenuListOption;

    case ax::mojom::Role::kRadioGroup:
      return item_role == ax::mojom::Role::kRadioButton;

    case ax::mojom::Role::kDescriptionList:
      // Only the term for each description list entry should receive posinset
      // and setsize.
      return item_role == ax::mojom::Role::kDescriptionListTerm ||
             item_role == ax::mojom::Role::kTerm;

    default:
      return false;
  }
}

// Finds ordered set that immediately contains node.
// Is not required for set's role to match node's role.
AXNode* AXNode::GetOrderedSet() const {
  AXNode* result = parent();

  // Continue walking up while parent is invalid, ignored, or is a generic
  // container.
  while (result && (result->data().HasState(ax::mojom::State::kIgnored) ||
                    result->data().role == ax::mojom::Role::kGenericContainer ||
                    result->data().role == ax::mojom::Role::kIgnored)) {
    result = result->parent();
  }
  return result;
}

}  // namespace ui
