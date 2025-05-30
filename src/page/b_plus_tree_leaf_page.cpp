#include "page/b_plus_tree_leaf_page.h"

#include <algorithm>

#include "index/generic_key.h"

#define pairs_off (data_)
#define pair_size (GetKeySize() + sizeof(RowId))
#define key_off 0
#define val_off GetKeySize()
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * TODO: Student Implement
 */
/**
 * Init method after creating a new leaf page
 * Including set page type, set current size to zero, set page id/parent id, set
 * next page id and set max size
 * 未初始化next_page_id
 */
void LeafPage::Init(page_id_t page_id, page_id_t parent_id, int key_size, int max_size) {
  SetPageType(IndexPageType::LEAF_PAGE);
  SetKeySize(key_size);
  SetSize(0);
  SetMaxSize(max_size);
  SetParentPageId(parent_id);
  SetPageId(page_id);
  SetNextPageId(INVALID_PAGE_ID);  // Initialize next page id to invalid
}

/**
 * Helper methods to set/get next page id
 */
page_id_t LeafPage::GetNextPageId() const { return next_page_id_; }

void LeafPage::SetNextPageId(page_id_t next_page_id) {
  next_page_id_ = next_page_id;
  if (next_page_id == 0) {
    LOG(INFO) << "Fatal error";
  }
}

/**
 * TODO: Student Implement
 */
/**
 * Helper method to find the first index i so that pairs_[i].first >= key
 * NOTE: This method is only used when generating index iterator // ??? Actually, it is used in insertion and deletion as well
 * 二分查找
 */
int LeafPage::KeyIndex(const GenericKey *key, const KeyManager &KM) {
  if (GetSize() == 0) return -1;

  int left = 0;
  int right = GetSize() - 1;
  while (left < right) {
    int mid = (left + right) / 2;
    if (KM.CompareKeys(key, KeyAt(mid)) <= 0) {
      right = mid;
    } else {
      left = mid + 1;
    }
  }
  if (KM.CompareKeys(key, KeyAt(left)) <= 0) {
    return left;
  } else {
    return left + 1;  // Return the next index if key is greater than the last key
  }
}

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
GenericKey *LeafPage::KeyAt(int index) {
  return reinterpret_cast<GenericKey *>(pairs_off + index * pair_size + key_off);
}

void LeafPage::SetKeyAt(int index, GenericKey *key) {
  memcpy(pairs_off + index * pair_size + key_off, key, GetKeySize());
}

RowId LeafPage::ValueAt(int index) const {
  return *reinterpret_cast<const RowId *>(pairs_off + index * pair_size + val_off);
}

void LeafPage::SetValueAt(int index, RowId value) {
  *reinterpret_cast<RowId *>(pairs_off + index * pair_size + val_off) = value;
}

void *LeafPage::PairPtrAt(int index) { return KeyAt(index); }

void LeafPage::PairCopy(void *dest, void *src, int pair_num) {
  memcpy(dest, src, pair_num * (GetKeySize() + sizeof(RowId)));
}
/*
 * Helper method to find and return the key & value pair associated with input
 * "index"(a.k.a. array offset)
 */
std::pair<GenericKey *, RowId> LeafPage::GetItem(int index) { return {KeyAt(index), ValueAt(index)}; }

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert key & value pair into leaf page ordered by key
 * @return page size after insertion
 */
int LeafPage::Insert(GenericKey *key, const RowId &value, const KeyManager &KM) {
  if (GetSize() == 0) {
    // The page is empty, insert at the beginning
    SetKeyAt(0, key);
    SetValueAt(0, value);
  } else {
    int index = KeyIndex(key, KM);
    PairCopy(PairPtrAt(index + 1), PairPtrAt(index), GetSize() - index);
    SetKeyAt(index, key);
    SetValueAt(index, value);
  }
  IncreaseSize(1);   // Increase the size of the page after insertion
  return GetSize();  // Return the new size of the page
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/

// NOTE: All move and copy methods of split add new key & value pairs to the end of the page

/*
 * Remove half of key & value pairs from this page to "recipient" page
 */
void LeafPage::MoveHalfTo(LeafPage *recipient) {
  int size = GetSize();
  int half_size = (size + 1) / 2;
  recipient->CopyNFrom(PairPtrAt(half_size), size - half_size);
  SetSize(half_size);  // Update the size of the current page
}

/*
 * Copy starting from items, and copy {size} number of elements into me.
 */
void LeafPage::CopyNFrom(void *src, int size) {
  PairCopy(PairPtrAt(GetSize()), src, size);
  IncreaseSize(size);
}

/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/*
 * For the given key, check to see whether it exists in the leaf page. If it
 * does, then store its corresponding value in input "value" and return true.
 * If the key does not exist, then return false
 */
bool LeafPage::Lookup(const GenericKey *key, RowId &value, const KeyManager &KM) {
  int index = KeyIndex(key, KM);
  if (index < GetSize() && KM.CompareKeys(KeyAt(index), key) == 0) {
    value = ValueAt(index);
    return true;  // Key found
  }
  return false;  // Key not found
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * First look through leaf page to see whether delete key exist or not. If
 * existed, perform deletion, otherwise return immediately.
 * NOTE: store key&value pair continuously after deletion
 * @return  page size after deletion
 */
int LeafPage::RemoveAndDeleteRecord(const GenericKey *key, const KeyManager &KM) {
  int index = KeyIndex(key, KM);
  if (index == -1) return GetSize();  // Key not found

  // Shift items to fill the gap
  PairCopy(PairPtrAt(index), PairPtrAt(index + 1), GetSize() - index - 1);
  IncreaseSize(-1);
  return GetSize();
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all key & value pairs from this page to "recipient" page. Don't forget
 * to update the next_page id in the sibling page
 */
void LeafPage::MoveAllTo(LeafPage *recipient) {
  recipient->CopyNFrom(PairPtrAt(0), GetSize());
  recipient->SetNextPageId(GetNextPageId());  // Update next page id
  SetSize(0);                                 // Clear the current page
  SetNextPageId(INVALID_PAGE_ID);             // Reset next page id
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to "recipient" page.
 *
 */
void LeafPage::MoveFirstToEndOf(LeafPage *recipient) {
  recipient->CopyLastFrom(KeyAt(0), ValueAt(0));
  PairCopy(PairPtrAt(0), PairPtrAt(1), GetSize() - 1);
  IncreaseSize(-1);
}

/*
 * Copy the item into the end of my item list. (Append item to my array)
 */
void LeafPage::CopyLastFrom(GenericKey *key, const RowId value) {
  int size = GetSize();
  SetKeyAt(size, key);
  SetValueAt(size, value);
  IncreaseSize(1);
}

/*
 * Remove the last key & value pair from this page to "recipient" page.
 */
void LeafPage::MoveLastToFrontOf(LeafPage *recipient) {
  int size = GetSize();
  recipient->CopyFirstFrom(KeyAt(size - 1), ValueAt(size - 1));
  IncreaseSize(-1);
}

/*
 * Insert item at the front of my items. Move items accordingly.
 *
 */
void LeafPage::CopyFirstFrom(GenericKey *key, const RowId value) {
  PairCopy(PairPtrAt(1), PairPtrAt(0), GetSize());
  SetKeyAt(0, key);
  SetValueAt(0, value);
  IncreaseSize(1);
}