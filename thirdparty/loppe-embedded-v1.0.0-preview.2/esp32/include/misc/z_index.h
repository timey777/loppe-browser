/* @loppe-copyright v3
 * Copyright (c) 2026 liguodong. All rights reserved.
 * Author: liguodong | GitHub: timey777
 * Unauthorized use, reproduction, or distribution is prohibited.
 */

#pragma once

#include <algorithm>

namespace loppe {

// 同级兄弟的叠放顺序（DSL `z-index`）：z 大的画在上面，同值保持声明顺序（后声明的在上）。
class ZIndex {
public:
    static constexpr int kDefault = 0;

    // lhs 是否画在 rhs 下面（相等返回 false：同值交给稳定排序保持原有先后）
    static bool paintsBelow(int lhs, int rhs);

    // 叠放位置是否相同
    static bool same(int lhs, int rhs);

    // 在 [first, last)（已按 z 升序）里找第一个比 z 靠上的兄弟位置；没有就返回 last。
    // zOf(element) 取该元素的 z-index。
    //   auto pos = ZIndex::insertPosition(children.begin(), children.end(), z, zOf);
    //   children.insert(pos, node);
    template <class It, class ZOf>
    static It insertPosition(It first, It last, int z, ZOf zOf) {
        for (; first != last; ++first) {
            if (paintsBelow(z, zOf(*first))) return first;
        }
        return last;
    }

    // 把 [first, last) 按 z 升序稳定排序（同值保持原有相对顺序）。
    template <class It, class ZOf>
    static void sortSiblings(It first, It last, ZOf zOf) {
        std::stable_sort(first, last, [&zOf](const auto& lhs, const auto& rhs) {
            return paintsBelow(zOf(lhs), zOf(rhs));
        });
    }
};

} // namespace loppe
