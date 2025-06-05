#include "buffer/clock_replacer.h"
#include <glog/logging.h>

CLOCKReplacer::CLOCKReplacer(size_t num_pages) : capacity(num_pages) {}

CLOCKReplacer::~CLOCKReplacer() = default;

// 选择一个可以被替换的页
bool CLOCKReplacer::Victim(frame_id_t *frame_id) {
    if (clock_list.empty()) {
        return false;  // 没有可以被替换的页
    }

    while (!clock_list.empty()) {
        frame_id_t current_frame = clock_list.front();
        clock_list.pop_front();

        // 如果当前页是未被pin的页
        if (clock_status[current_frame] == 0) {
            *frame_id = current_frame;
            clock_status.erase(current_frame);
            return true;  // 找到一个可以被替换的页
        } else {
            // 将当前页的状态设置为未使用，并将其放回队列末尾
            clock_status[current_frame] = 0;  // 设置为未使用状态
            clock_list.push_back(current_frame);
        }
    }
    return false;  // 没有找到可以被替换的页
}
void CLOCKReplacer::Pin(frame_id_t frame_id) {
    // 如果页存在于replacer中，将其状态设置为未使用
    if (clock_status.find(frame_id) != clock_status.end()) {
        clock_list.remove(frame_id);  // 从列表中移除该页
        clock_status.erase(frame_id);  // 从状态映射中移除该页
    }
}
void CLOCKReplacer::Unpin(frame_id_t frame_id) {
    if(clock_list.size() > capacity) {
        LOG(ERROR) << "CLOCKReplacer is over capacity: " << clock_list.size() << " > " << capacity;
        return;  
    }

    if (clock_status.find(frame_id) != clock_status.end()) {
        // 如果页已经存在于replacer中，将其状态设置为使用
        clock_status[frame_id] = 1;  // 设置为使用状态
    } else {
        if(clock_list.size() == capacity) {
        frame_id_t victim_frame_id;
        if (!Victim(&victim_frame_id)) {
            LOG(ERROR) << "Cannot unpin page " << frame_id << ": Capacity Full And Victim Failed";
            return;  // 👈 Victim失败时应该返回
        }
        }
        // 如果页不存在于replacer中，添加它
        if (clock_list.size() < capacity) {
            clock_list.push_back(frame_id);
            clock_status[frame_id] = 1;  // 设置为使用状态
        }
    }
}
size_t CLOCKReplacer::Size() {
    return clock_list.size();  // 返回当前可以被替换的页数
}
