#include "index/b_plus_tree.h"

#include <string>

#include "glog/logging.h"
#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/index_roots_page.h"

/**
 * TODO: Student Implement
 */
BPlusTree::BPlusTree(index_id_t index_id, BufferPoolManager *buffer_pool_manager, const KeyManager &KM,
                     int leaf_max_size, int internal_max_size)
    : index_id_(index_id),
      buffer_pool_manager_(buffer_pool_manager),
      processor_(KM),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {
  // NOTE: size == leaf_max_size_ or internal_max_size_ means overflow, not >
  if (leaf_max_size_ == UNDEFINED_SIZE || internal_max_size_ == UNDEFINED_SIZE) {
    leaf_max_size_ = (PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / (KM.GetKeySize() + sizeof(RowId));
  }
  if (internal_max_size_ == UNDEFINED_SIZE) {
    internal_max_size_ = (PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (KM.GetKeySize() + sizeof(page_id_t));
  }
}

void BPlusTree::Destroy(page_id_t current_page_id) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
bool BPlusTree::IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
bool BPlusTree::GetValue(const GenericKey *key, std::vector<RowId> &result, Txn *transaction) {
  if (IsEmpty()) return false;

  auto *leaf_page = reinterpret_cast<LeafPage *>(FindLeafPage(key, root_page_id_));
  if (leaf_page == nullptr) {
    LOG(ERROR) << "Leaf page not found";
    return false;  // Leaf page not found
  }
  RowId value;

  // Check if the key exists in the leaf page
  if (!leaf_page->Lookup(key, value, processor_)) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);  // Unpin the leaf page without dirty flag
    return false;                                                    // Key not found
  } else {
    result.push_back(value);                                         // Add the found value to the result vector
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);  // Unpin the leaf page without dirty flag
    return true;                                                     // Key found and value added to result
  }
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
bool BPlusTree::Insert(GenericKey *key, const RowId &value, Txn *transaction) {
  if (IsEmpty()) {
    StartNewTree(key, value);
    return true;
  } else {
    return InsertIntoLeaf(key, value, transaction);
  }
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
void BPlusTree::StartNewTree(GenericKey *key, const RowId &value) {
  // Allocate a new leaf page
  auto *page = buffer_pool_manager_->NewPage(root_page_id_);
  if (page == nullptr) throw("Out of memory: Unable to allocate new leaf page.");
  UpdateRootPageId(1);  // Insert the record of the new root page

  auto *leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  // Initialize the leaf page
  leaf_page->Init(root_page_id_, INVALID_PAGE_ID, processor_.GetKeySize(), leaf_max_size_);

  leaf_page->Insert(key, value, processor_);

  buffer_pool_manager_->UnpinPage(root_page_id_, true);  // Unpin the page after insertion
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immediately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
bool BPlusTree::InsertIntoLeaf(GenericKey *key, const RowId &value, Txn *transaction) {
  // Find the right leaf page for insertion
  auto *leaf_page = reinterpret_cast<LeafPage *>(FindLeafPage(key, root_page_id_));
  if (leaf_page == nullptr) {
    LOG(ERROR) << "Leaf page not found";
    return false;  // Leaf page not found
  }

  // Check if the key already exists in the leaf page
  RowId tmp_value;
  if (leaf_page->Lookup(key, tmp_value, processor_)) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);  // Unpin the leaf page without dirty flag
    return false;                                                    // Key already exists, do not insert
  } else {
    // Insert entry and check if it exceeds the maximum size
    int size = leaf_page->Insert(key, value, processor_);
    if (size >= leaf_max_size_) {
      // Split the leaf page if it exceeds the maximum size
      auto *new_leaf_page = Split(leaf_page, transaction);
      if (new_leaf_page == nullptr) {
        throw("Out of memory: Unable to split leaf page.");
      }
      // Insert the new page into the parent
      InsertIntoParent(leaf_page, new_leaf_page->KeyAt(0), new_leaf_page, transaction);
    }
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);  // Unpin the leaf page after insertion
    return true;                                                    // Insertion successful
  }
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
BPlusTreeInternalPage *BPlusTree::Split(InternalPage *node, Txn *transaction) {
  // Allocate a new internal page
  page_id_t page_id;
  auto *new_page = buffer_pool_manager_->NewPage(page_id);
  if (new_page == nullptr) throw("Out of memory: Unable to allocate new internal page.");

  auto *recipient = reinterpret_cast<InternalPage *>(new_page->GetData());
  recipient->Init(page_id, node->GetParentPageId(), processor_.GetKeySize(), internal_max_size_);

  // Move half of the entries from the old node to the new node
  node->MoveHalfTo(recipient, buffer_pool_manager_);
  // Update the parent page ID of the recipient
  recipient->SetParentPageId(node->GetParentPageId());
  // Unpin the recipient page after moving entries
  buffer_pool_manager_->UnpinPage(page_id, true);

  return recipient;
}

BPlusTreeLeafPage *BPlusTree::Split(LeafPage *node, Txn *transaction) {
  // Allocate a new leaf page
  page_id_t page_id;
  auto *new_page = buffer_pool_manager_->NewPage(page_id);
  if (new_page == nullptr) throw("Out of memory: Unable to allocate new leaf page.");

  auto *recipient = reinterpret_cast<LeafPage *>(new_page->GetData());
  recipient->Init(page_id, node->GetParentPageId(), processor_.GetKeySize(), leaf_max_size_);

  // Move half of the entries from the old node to the new node
  node->MoveHalfTo(recipient);
  // Update the next page ID of the new node
  recipient->SetNextPageId(node->GetNextPageId());
  node->SetNextPageId(recipient->GetPageId());
  // Unpin the recipient page after moving entries
  buffer_pool_manager_->UnpinPage(page_id, true);

  return recipient;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
void BPlusTree::InsertIntoParent(BPlusTreePage *old_node, GenericKey *key, BPlusTreePage *new_node, Txn *transaction) {
  if (old_node->IsRootPage()) {
    auto *page = buffer_pool_manager_->NewPage(root_page_id_);
    if (page == nullptr) throw("Out of memory: Unable to allocate new root page.");
    UpdateRootPageId(0);  // Update the root page ID

    auto *root = reinterpret_cast<InternalPage *>(page->GetData());
    root->Init(root_page_id_, INVALID_PAGE_ID, processor_.GetKeySize(), internal_max_size_);
    root->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
    // Set the parent page ID for the new node
    old_node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);

    // Unpin the new root page after insertion
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
  } else {
    // Find the parent page of the old node
    int parent_page_id = old_node->GetParentPageId();
    auto *page = buffer_pool_manager_->FetchPage(parent_page_id);
    auto *parent = reinterpret_cast<InternalPage *>(page->GetData());
    parent->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    if (parent->GetSize() >= internal_max_size_) {
      // If the parent page is overflowing, split it
      auto *recipient = Split(parent, transaction);
      if (recipient == nullptr) throw("Out of memory: Unable to split parent page.");
      // Insert the new key into the parent of the parent
      InsertIntoParent(parent, recipient->KeyAt(0), recipient, transaction);
    }
    buffer_pool_manager_->UnpinPage(parent_page_id, true);  // Unpin the parent page after insertion
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
void BPlusTree::Remove(const GenericKey *key, Txn *transaction) {
  if (IsEmpty()) return;  // If the tree is empty, return immediately

  auto *leaf_page = FindLeafPage(key, root_page_id_);
  auto *leaf = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  if (leaf == nullptr) {
    LOG(ERROR) << "Leaf page not found for key: " << key;
    return;  // Leaf page not found
  }

  // If we delete the first key in the leaf page, we need to revise corresponding parent page
  bool is_first_key = (leaf->KeyIndex(key, processor_) == 0);
  leaf->RemoveAndDeleteRecord(key, processor_);
  if (is_first_key && !leaf->IsRootPage()) {
    auto *parent_page = buffer_pool_manager_->FetchPage(leaf->GetParentPageId());
    auto *parent = reinterpret_cast<InternalPage *>(parent_page->GetData());
    page_id_t value = leaf->GetPageId();
    int index;
    while ((index = parent->ValueIndex(value)) == 0 && !parent->IsRootPage()) {
      buffer_pool_manager_->UnpinPage(parent->GetPageId(), false);  // Unpin the parent page without dirty flag
      value = parent->GetPageId();
      parent_page = buffer_pool_manager_->FetchPage(parent->GetParentPageId());
      parent = reinterpret_cast<InternalPage *>(parent_page->GetData());
    }
    if (index > 0) {
      parent->SetKeyAt(index, leaf->KeyAt(0));  // Update the key in the parent page
    }
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);  // Unpin the parent page after updating
  }

  // Check if the leaf page is underflowed
  if (leaf->GetSize() < leaf->GetMinSize()) {
    CoalesceOrRedistribute(leaf, transaction);
  }
}

/* todo
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
template <typename N>
bool BPlusTree::CoalesceOrRedistribute(N *&node, Txn *transaction) {
  // If the node is the root, adjust it
  if (node->IsRootPage()) return AdjustRoot(node);

  // Fetch the parent page of the node
  page_id_t parent_page_id = node->GetParentPageId();
  auto *parent_page = buffer_pool_manager_->FetchPage(parent_page_id);
  auto *parent = reinterpret_cast<InternalPage *>(parent_page->GetData());
  int index = parent->ValueIndex(node->GetPageId());
  if (index < 0) {
    buffer_pool_manager_->UnpinPage(parent_page_id, false);  // Unpin the parent page without dirty flag
    return false;                                            // Node not found in parent
  }

  // Find the sibling page of the node
  page_id_t sibling_page_id = (index == 0) ? parent->ValueAt(1) : parent->ValueAt(index - 1);
  auto *sibling_page = buffer_pool_manager_->FetchPage(sibling_page_id);
  auto *sibling = reinterpret_cast<N *>(sibling_page->GetData());

  // Check if the sibling can redistribute or coalesce
  if (sibling->GetSize() + node->GetSize() >= sibling->GetMaxSize()) {
    // If the sibling can redistribute, do so
    Redistribute(sibling, node, index);
    buffer_pool_manager_->UnpinPage(sibling_page_id, true);  // Unpin the sibling page after redistribution
    buffer_pool_manager_->UnpinPage(parent_page_id, true);   // Unpin the parent page after redistribution
    return false;                                            // No deletion happened
  } else {
    // If the sibling cannot redistribute, coalesce
    bool should_delete = Coalesce(sibling, node, parent, index, transaction);
    buffer_pool_manager_->UnpinPage(sibling_page_id, true);  // Unpin the sibling page after coalescing
    buffer_pool_manager_->UnpinPage(parent_page_id, true);   // Unpin the parent page after coalescing
    return should_delete;                                    // Return whether the node should be deleted
  }
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion happened
 */
bool BPlusTree::Coalesce(LeafPage *&neighbor_node, LeafPage *&node, InternalPage *&parent, int index,
                         Txn *transaction) {
  if (index == 0) {
    neighbor_node->MoveAllTo(node);                                     // Move all entries from neighbor to node
    buffer_pool_manager_->UnpinPage(neighbor_node->GetPageId(), true);  // Unpin the neighbor page
    buffer_pool_manager_->DeletePage(neighbor_node->GetPageId());       // Delete the neighbor page
    parent->Remove(1);  // Remove the key in the parent that points to the neighbor
    if (parent->GetSize() < parent->GetMinSize()) {
      // If the parent is underflowed, coalesce or redistribute
      return CoalesceOrRedistribute(parent, transaction);
    }
  } else {
    node->MoveAllTo(neighbor_node);                            // Move all entries from node to neighbor
    buffer_pool_manager_->UnpinPage(node->GetPageId(), true);  // Unpin the node page
    buffer_pool_manager_->DeletePage(node->GetPageId());       // Delete the node page
    parent->Remove(index);                                     // Remove the key in the parent that points to the node
    if (parent->GetSize() < parent->GetMinSize()) {
      // If the parent is underflowed, coalesce or redistribute
      return CoalesceOrRedistribute(parent, transaction);
    }
  }
}

bool BPlusTree::Coalesce(InternalPage *&neighbor_node, InternalPage *&node, InternalPage *&parent, int index,
                         Txn *transaction) {
  if (index == 0) {
    neighbor_node->MoveAllTo(node, parent->KeyAt(1), buffer_pool_manager_);  // Move all entries from neighbor to node
    buffer_pool_manager_->UnpinPage(neighbor_node->GetPageId(), true);       // Unpin the neighbor page
    buffer_pool_manager_->DeletePage(neighbor_node->GetPageId());            // Delete the neighbor page
    parent->Remove(1);  // Remove the key in the parent that points to the neighbor
    if (parent->GetSize() < parent->GetMinSize()) {
      // If the parent is underflowed, coalesce or redistribute
      return CoalesceOrRedistribute(parent, transaction);
    }
  } else {
    node->MoveAllTo(neighbor_node, parent->KeyAt(index),
                    buffer_pool_manager_);                     // Move all entries from node to neighbor
    buffer_pool_manager_->UnpinPage(node->GetPageId(), true);  // Unpin the node page
    buffer_pool_manager_->DeletePage(node->GetPageId());       // Delete the node page
    parent->Remove(index);                                     // Remove the key in the parent that points to the node
    if (parent->GetSize() < parent->GetMinSize()) {
      // If the parent is underflowed, coalesce or redistribute
      return CoalesceOrRedistribute(parent, transaction);
    }
  }
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
void BPlusTree::Redistribute(LeafPage *neighbor_node, LeafPage *node, int index) {
  page_id_t parent_page_id = node->GetParentPageId();
  auto *parent_page = buffer_pool_manager_->FetchPage(parent_page_id);
  auto *parent = reinterpret_cast<InternalPage *>(parent_page->GetData());

  if (index == 0) {
    // Move the first key-value pair from the neighbor to the end of the node
    neighbor_node->MoveFirstToEndOf(node);
    parent->SetKeyAt(1, neighbor_node->KeyAt(0));  // Update the parent key
  } else {
    // Move the last key-value pair from the neighbor to the beginning of the node
    neighbor_node->MoveLastToFrontOf(node);
    parent->SetKeyAt(index, node->KeyAt(0));  // Update the parent key
  }
  buffer_pool_manager_->UnpinPage(parent_page_id, true);  // Unpin the parent page after redistribution
}

void BPlusTree::Redistribute(InternalPage *neighbor_node, InternalPage *node, int index) {
  page_id_t parent_page_id = node->GetParentPageId();
  auto *parent_page = buffer_pool_manager_->FetchPage(parent_page_id);
  auto *parent = reinterpret_cast<InternalPage *>(parent_page->GetData());

  if (index == 0) {
    int index = parent->ValueIndex(neighbor_node->GetPageId());
    GenericKey *key = neighbor_node->KeyAt(1);
    neighbor_node->MoveFirstToEndOf(node, parent->KeyAt(index), buffer_pool_manager_);
    // NOTE: After executing MoveFirstToEndOf, the first key of neighbor_node is now at index 0
    parent->SetKeyAt(index, neighbor_node->KeyAt(0));  // Update the parent key
  } else {
    int index = parent->ValueIndex(node->GetPageId());
    GenericKey *key = neighbor_node->KeyAt(neighbor_node->GetSize() - 1);
    neighbor_node->MoveLastToFrontOf(node, parent->KeyAt(index), buffer_pool_manager_);
    parent->SetKeyAt(index, key);  // Update the parent key
  }
  buffer_pool_manager_->UnpinPage(parent_page_id, true);  // Unpin the parent page after redistribution
}

/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happened
 */
bool BPlusTree::AdjustRoot(BPlusTreePage *old_root_node) {
  // LOG(ERROR) << "Adjusting root page with ID: " << old_root_node->GetPageId();
  if (old_root_node->GetSize() > 1) return false;  // If the root has more than one key, no adjustment needed

  // If the root has only one key, we need to adjust the root
  if (old_root_node->IsLeafPage()) {
    // If the root is a leaf page and has only one key, we can simply delete it
    root_page_id_ = INVALID_PAGE_ID;
  } else {
    // If the root is an internal page with only one child, we need to promote that child
    auto *internal_page = reinterpret_cast<InternalPage *>(old_root_node);
    root_page_id_ = internal_page->RemoveAndReturnOnlyChild();
    auto *new_root_page = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(root_page_id_)->GetData());
    new_root_page->SetParentPageId(INVALID_PAGE_ID);
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
  }
  UpdateRootPageId(0);  // Update the root page ID in the header page
  return true;          // Root page deleted
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the left most leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
IndexIterator BPlusTree::Begin() {
  Page *page = FindLeafPage(nullptr, root_page_id_, true);
  if (page == nullptr) return IndexIterator();
  int page_id = page->GetPageId();
  buffer_pool_manager_->UnpinPage(page_id, false);  // Unpin the page without dirty flag
  return IndexIterator(page_id, buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
IndexIterator BPlusTree::Begin(const GenericKey *key) {
  Page *page = FindLeafPage(key, root_page_id_, false);
  if (page == nullptr) return IndexIterator();
  int page_id = page->GetPageId();
  auto *leaf = reinterpret_cast<LeafPage *>(page->GetData());
  buffer_pool_manager_->UnpinPage(page_id, false);  // Unpin the page without dirty flag
  return IndexIterator(page_id, buffer_pool_manager_,
                       leaf->KeyIndex(key, processor_));  // Create iterator at the key index
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
IndexIterator BPlusTree::End() { return IndexIterator(); }

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 * NOTE: the leaf page is pinned, you need to unpin it after use.
 */
Page *BPlusTree::FindLeafPage(const GenericKey *key, page_id_t page_id, bool leftMost) {
  if (IsEmpty()) return nullptr;

  auto *page = buffer_pool_manager_->FetchPage(page_id);

  auto *tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());

  if (tree_page->IsLeafPage()) return page;

  auto *internal_page = reinterpret_cast<InternalPage *>(tree_page);
  page_id_t next_page_id;
  if (leftMost)
    next_page_id = internal_page->ValueAt(0);
  else
    next_page_id = internal_page->Lookup(key, processor_);

  buffer_pool_manager_->UnpinPage(page_id, false);  // Unpin the current page
  return FindLeafPage(key, next_page_id, leftMost);
}

/*
 * Update/Insert root page id in header page(where page_id = INDEX_ROOTS_PAGE_ID,
 * header_page isdefined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      default value is false. When set to true,
 * insert a record <index_name, current_page_id> into header page instead of
 * updating it.
 */
void BPlusTree::UpdateRootPageId(int insert_record) {
  auto *header_page = buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID);

  auto *index_roots_page = reinterpret_cast<IndexRootsPage *>(header_page->GetData());
  if (insert_record) {
    index_roots_page->Insert(index_id_, root_page_id_);
  } else {
    index_roots_page->Update(index_id_, root_page_id_);
  }

  buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID, true);  // Unpin the header page
}

/**
 * This method is used for debug only, You don't need to modify
 */
void BPlusTree::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out, Schema *schema) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId()
        << ",Parent=" << leaf->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      Row ans;
      processor_.DeserializeToKey(leaf->KeyAt(i), ans, schema);
      out << "<TD>" << ans.GetField(0)->toString() << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId()
        << ",Parent=" << inner->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        Row ans;
        processor_.DeserializeToKey(inner->KeyAt(i), ans, schema);
        out << ans.GetField(0)->toString();
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out, schema);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 */
void BPlusTree::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
      bpm->UnpinPage(internal->ValueAt(i), false);
    }
  }
}

bool BPlusTree::Check() {
  bool all_unpinned = buffer_pool_manager_->CheckAllUnpinned();
  if (!all_unpinned) {
    LOG(ERROR) << "problem in page unpin" << endl;
  }
  return all_unpinned;
}
