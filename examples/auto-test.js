// auto-test.js —— 自驱动回归：挂载 → el.remove → 整页重渲染，循环多轮。
//
// 验证目标（引擎所有权修复的回归面）：
//   ① ui.append 的 snippet wrapper 在重渲染时被回收，且不误伤已挂载的组件节点；
//   ② el.remove 走延迟移除 + 帧末释放，和整页重渲染的 reclaimUnreachable 不打架；
//   ③ 多轮循环后控制台不出现「releaseOwnedNode: 节点不属于本树」/ 悬垂崩溃。

import doc from './auto-test.lui';

const maxRounds = 6;
let round = 0;

function mountAll(tag) {
  // ui.append 的 parentId 是裸 id（不带 #）——与 Component.mount 一致
  ui.append('slot', `panel(id="m1-${tag}", class="card") { text(value="${tag}-1") }`);
  ui.append('slot', `panel(id="m2-${tag}", class="card") { text(value="${tag}-2") }`);
  ui.append('slot', `panel(id="m3-${tag}", class="card") { text(value="${tag}-3") }`);
  console.log('mounted', tag);
}

function cycle() {
  round += 1;
  if (round > maxRounds) {
    ui.query('#status').text = '全部 ' + maxRounds + ' 轮通过';
    console.log('AUTO-TEST DONE');
    return;
  }
  // 整页换文档：differ + reclaimUnreachable + wrapper 回收（翻页同款路径）
  ui.render(doc);
  const tag = 'r' + round;
  mountAll(tag);
  setTimeout(() => {
    // 延迟移除路径：当帧摘除 + 帧末释放
    const el = ui.query('#m2-' + tag);
    if (el) {
      el.remove();
      console.log('removed m2 at', tag);
    } else {
      console.log('MISSING m2 at', tag);
    }
    setTimeout(cycle, 500);
  }, 600);
}

ui.render(doc);
console.log('auto-test start');
setTimeout(cycle, 400);
