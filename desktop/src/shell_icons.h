#pragma once

// 外壳自用图标：注册到引擎的 SVG 图标表（svg::RegisterIcon），供 ui/main.lui 与
// 标签条动态元素里的 icon(name="...") 引用。
//
// 名字约定（kernel 侧直接拼字符串，改名字要两边一起改）：
//   shell-reload       刷新按钮
//   shell-back/forward 前进 / 后退（main.lui 里用）
//   shell-close        标签关闭
//   shell-min          最小化
//   shell-max          最大化
//   shell-restore      还原（最大化后 win-max 图标换成它，见 BrowserScene::refreshWindowButtons）
//   shell-win-close    窗口关闭
//   shell-tab-globe    标签的默认图标（地球；有 favicon 时被替换）
//   shell-spin-0..7    加载转圈，8 帧（kernel 按 80ms 一帧轮换）
//
// 图形本身是白色掩码，颜色由使用处的 CSS color 定（见 main.lui 的 .navicon / .winicon）。

namespace loppe_browser {

// 启动时调一次（Application::build 里，控件建树之前）。
void RegisterShellIcons();

} // namespace loppe_browser
