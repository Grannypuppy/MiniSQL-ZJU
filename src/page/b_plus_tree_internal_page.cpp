#include "page/b_plus_tree_internal_page.h"

#include "index/generic_key.h"

#define pairs_off (data_)
#define pair_size (GetKeySize() + sizeof(page_id_t))
#define key_off 0
#define val_off GetKeySize()

/**
 * TODO: Student Implement
 */
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set key size, set current size, set page id,
 * set parent id and set max page size
 */
void InternalPage::Init(page_id_t page_id, page_id_t parent_id, int key_size, int max_size) {
  SetPageType(IndexPageType::INTERNAL_PAGE);
  SetKeySize(key_size);
  SetSize(0);
  SetMaxSize(max_size);
  SetParentPageId(parent_id);
  SetPageId(page_id);

  // ?? maybe_unused
  // Initialize the first key as invalid
  // GenericKey *dummy_key = KeyAt(0);
  // memset(dummy_key, 0, GetKeySize());
}
/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
GenericKey *InternalPage::KeyAt(int index) {
  return reinterpret_cast<GenericKey *>(pairs_off + index * pair_size + key_off);
}

void InternalPage::SetKeyAt(int index, GenericKey *key) {
  memcpy(pairs_off + index * pair_size + key_off, key, GetKeySize());
}

page_id_t InternalPage::ValueAt(int index) const {
  return *reinterpret_cast<const page_id_t *>(pairs_off + index * pair_size + val_off);
}

void InternalPage::SetValueAt(int index, page_id_t value) {
  *reinterpret_cast<page_id_t *>(pairs_off + index * pair_size + val_off) = value;
}

int InternalPage::ValueIndex(const page_id_t &value) const {
  for (int i = 0; i < GetSize(); ++i) {
    if (ValueAt(i) == value) return i;
  }
  return -1;
}

void *InternalPage::PairPtrAt(int index) { return KeyAt(index); }

void InternalPage::PairCopy(void *dest, void *src, int pair_num) {
  memcpy(dest, src, pair_num * (GetKeySize() + sizeof(page_id_t)));
}
/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/*
 * Find and return the child pointer(page_id) which points to the child page
 * that contains input "key"
 * Start the search from the second key(the first key should always be invalid)
 * 用了二分查找
 */
page_id_t InternalPage::Lookup(const GenericKey *key, const KeyManager &KM) {
  if (GetSize() == 0) return INVALID_PAGE_ID;

  int left = 1;
  int right = GetSize();
  while (left < right) {
    int mid = (left + right) / 2;
    if (KM.CompareKeys(key, KeyAt(mid)) >= 0) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  return ValueAt(left - 1);
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Populate new root page with old_value + new_key & new_value
 * When the insertion cause overflow from leaf page all the way upto the root
 * page, you should create a new root page and populate its elements.
 * NOTE: This method is only called within InsertIntoParent()(b_plus_tree.cpp)
 */
void InternalPage::PopulateNewRoot(const page_id_t &old_value, GenericKey *new_key, const page_id_t &new_value) {
  SetSize(2);                // New root will have two entries
  SetValueAt(0, old_value);  // Set the old value at index 0
  SetKeyAt(1, new_key);      // Set the new key at index 1
  SetValueAt(1, new_value);  // Set the new value at index 1
}

/*
 * Insert new_key & new_value pair right after the pair with its value ==
 * old_value
 *
 * Set old_value as `INVALID_PAGE_ID` when inserting the new key-value pair at the beginning
 * @return:  new size after insertion
 */
int InternalPage::InsertNodeAfter(const page_id_t &old_value, GenericKey *new_key, const page_id_t &new_value) {
  int index = ValueIndex(old_value);
  // Shift elements to the right to make space for the new key-value pair
  PairCopy(PairPtrAt(index + 2), PairPtrAt(index + 1), GetSize() - index - 1);

  // Insert the new key-value pair
  if (index == -1)
    SetKeyAt(1, new_key);  // Insert at the beginning if old_value is not found
  else
    SetKeyAt(index + 1, new_key);
  SetValueAt(index + 1, new_value);
  IncreaseSize(1);  // Increase the size of the internal page

  return GetSize();
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/

// NOTE: All move and copy methods of split add new key & value pairs to the end of the page

 /*
 * Remove half of key & value pairs from this page to "recipient" page
 * buffer_pool_manager 是干嘛的？传给CopyNFrom()用于Fetch数据页
 */
void InternalPage::MoveHalfTo(InternalPage *recipient, BufferPoolManager *buffer_pool_manager) {
  int size = GetSize();
  int half_size = (size + 1) / 2;  // Calculate the half size, rounding up
  recipient->CopyNFrom(PairPtrAt(half_size), GetSize() - half_size, buffer_pool_manager);
  SetSize(half_size);  // Update the size of the current page
  LOG(INFO) << "Internal page " << GetPageId() << " split, new size: " << GetSize()
            << ", recipient page size: " << recipient->GetSize();
}

/* Copy entries into me, starting from {src} and copy {size} entries.
 * Since it is an internal page, for all entries (pages) moved, their parents page now changes to me.
 * So I need to 'adopt' them by changing their parent page id, which needs to be persisted with BufferPoolManger
 *
 * @param src: pointer to the continuous key-value pairs to be copied
 */
void InternalPage::CopyNFrom(void *src, int size, BufferPoolManager *buffer_pool_manager) {
  // Copy the key-value pairs from src to the current page
  int start_index = GetSize();  // Start index for copying
  PairCopy(PairPtrAt(start_index), src, size);
  // if (start_index == 0) memset(KeyAt(0), 0, GetKeySize());  // Initialize the first key if it's the first insertion
  IncreaseSize(size);  // Increase the size of the internal page

  // Update the parent page id for each copied child page
  for (int i = start_index; i < start_index + size; ++i) {
    int page_id = ValueAt(i);
    auto *child_page = buffer_pool_manager->FetchPage(page_id);
    if (child_page == nullptr) {
      LOG(ERROR) << "Failed to fetch child page with id: " << page_id;
      return;
    }
    auto *node = reinterpret_cast<BPlusTreeInternalPage *>(child_page->GetData());
    node->SetParentPageId(GetPageId());
    buffer_pool_manager->UnpinPage(page_id, true);  // Unpin the child page after updating
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Remove the key & value pair in internal page according to input index(a.k.a
 * array offset)
 * NOTE: store key&value pair continuously after deletion
 * NOTE: not need to set the first key to invalid, as we get the original second
 * key by call KeyAt(0) after Remove in the function BPlusTree::Redistribute
 */
void InternalPage::Remove(int index) {
  PairCopy(PairPtrAt(index), PairPtrAt(index + 1), GetSize() - index - 1);
  IncreaseSize(-1);  // Decrease the size of the internal page
}

/*
 * Remove the only key & value pair in internal page and return the value
 * NOTE: only call this method within AdjustRoot()(in b_plus_tree.cpp)
 * And in this case, we should retain the first value as the only child
 */
page_id_t InternalPage::RemoveAndReturnOnlyChild() {
  if (GetSize() != 1) {
    LOG(ERROR) << "Cannot remove and return only child from an internal page with size: " << GetSize();
    return INVALID_PAGE_ID;
  }

  Remove(0);

  return ValueAt(0);
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all of key & value pairs from this page to "recipient" page.
 * The middle_key is the separation key you should get from the parent. You need
 * to make sure the middle key is added to the recipient to maintain the invariant.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those
 * pages that are moved to the recipient
 *
 * NOTE: Caller should ensure that the recipient page is smaller than this page.
 */
void InternalPage::MoveAllTo(InternalPage *recipient, GenericKey *middle_key, BufferPoolManager *buffer_pool_manager) {
  recipient->CopyLastFrom(middle_key, ValueAt(0), buffer_pool_manager);
  recipient->CopyNFrom(PairPtrAt(1), GetSize() - 1, buffer_pool_manager);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to tail of "recipient" page.
 *
 * The middle_key is the separation key you should get from the parent. You need
 * to make sure the middle key is added to the recipient to maintain the invariant.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those
 * pages that are moved to the recipient
 */
void InternalPage::MoveFirstToEndOf(InternalPage *recipient, GenericKey *middle_key,
                                    BufferPoolManager *buffer_pool_manager) {
  recipient->CopyLastFrom(middle_key, ValueAt(0), buffer_pool_manager);
  Remove(0);  // Remove the first key-value pair from this page
}

/* Append an entry at the end.
 * Since it is an internal page, the moved entry(page)'s parent needs to be updated.
 * So I need to 'adopt' it by changing its parent page id, which needs to be persisted with BufferPoolManger
 */
void InternalPage::CopyLastFrom(GenericKey *key, const page_id_t value, BufferPoolManager *buffer_pool_manager) {
  // Insert the new key-value pair at the end
  SetKeyAt(GetSize(), key);
  SetValueAt(GetSize(), value);
  IncreaseSize(1);  // Increase the size of the internal page

  // Update the parent page id of the value being copied
  auto *child_page = buffer_pool_manager->FetchPage(value);
  auto *child = reinterpret_cast<BPlusTreeInternalPage *>(child_page->GetData());
  child->SetParentPageId(GetPageId());
  buffer_pool_manager->UnpinPage(value, true);  // Unpin the child page after updating
}

/*
 * Remove the last key & value pair from this page to head of "recipient" page.
 * You need to handle the original dummy key properly, e.g. updating recipient’s array to position the middle_key at the
 * right place.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those pages that are
 * moved to the recipient
 */
void InternalPage::MoveLastToFrontOf(InternalPage *recipient, GenericKey *middle_key,
                                     BufferPoolManager *buffer_pool_manager) {
  recipient->CopyFirstFrom(ValueAt(GetSize() - 1), buffer_pool_manager);
  recipient->SetKeyAt(1, middle_key);  // Set the middle key at the front of recipient
  Remove(GetSize() - 1);               // Remove the last key-value pair from this page
}

/* Append an entry at the beginning.
 * Since it is an internal page, the moved entry(page)'s parent needs to be updated.
 * So I need to 'adopt' it by changing its parent page id, which needs to be persisted with BufferPoolManger
 *
 * NOTE: Need to set the second key(use SetKeyAt(1, key)) after this function is called,
 */
void InternalPage::CopyFirstFrom(const page_id_t value, BufferPoolManager *buffer_pool_manager) {
  InsertNodeAfter(INVALID_PAGE_ID, KeyAt(0), value);

  // Update the parent page id of the value being copied
  auto *child_page = buffer_pool_manager->FetchPage(value);
  auto *child = reinterpret_cast<BPlusTreeInternalPage *>(child_page->GetData());
  child->SetParentPageId(GetPageId());
  buffer_pool_manager->UnpinPage(value, true);  // Unpin the child page after updating
}