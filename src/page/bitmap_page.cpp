#include "page/bitmap_page.h"

#include "glog/logging.h"

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::AllocatePage(uint32_t &page_offset) {
  auto max_size = GetMaxSupportedSize();

  if (page_allocated_ >= max_size) {
    //LOG(WARNING) << "Page is full, cannot allocate more pages";
    return false;
  }

  // 分配页
  uint32_t byte_idx = next_free_page_ / 8;
  uint32_t bit_idx = next_free_page_ % 8;
  page_allocated_++;
  page_offset = next_free_page_;
  bytes[byte_idx] |= (1 << bit_idx);

  // 查找下一可用页
  for (uint32_t i = page_offset; i < max_size; i++) {
    if (IsPageFree(i)) {
      next_free_page_ = i;
      break;
    }
  }
  return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::DeAllocatePage(uint32_t page_offset) {
  auto max_size = GetMaxSupportedSize();

  if (page_offset >= max_size) {
    LOG(WARNING) << "Error page offset: " << page_offset << " >= " << max_size;
    return false;
  }

  if (IsPageFree(page_offset)) {
    // LOG(WARNING) << "Page " << page_offset << " is already free";
    return false;
  }

  // 释放页
  uint32_t byte_idx = page_offset / 8;
  uint32_t bit_idx = page_offset % 8;
  bytes[byte_idx] &= ~(1 << bit_idx);
  page_allocated_--;

  if (page_offset < next_free_page_) next_free_page_ = page_offset;

  return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFree(uint32_t page_offset) const {
  auto max_size = GetMaxSupportedSize();
  if (page_offset >= max_size) {
    LOG(WARNING) << "ERROR PAGE_OFFSET";
    return false;
  }

  return IsPageFreeLow(page_offset / 8, page_offset % 8);
}

template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFreeLow(uint32_t byte_index, uint8_t bit_index) const {
  if (byte_index >= MAX_CHARS || bit_index >= 8) {
    LOG(WARNING) << "BYTE_INDEX OR BIT_INDEX IS TOO BIG";
    return false;
  }

  return (bytes[byte_index] & (1 << bit_index)) == 0;
}

template class BitmapPage<64>;

template class BitmapPage<128>;

template class BitmapPage<256>;

template class BitmapPage<512>;

template class BitmapPage<1024>;

template class BitmapPage<2048>;

template class BitmapPage<4096>;