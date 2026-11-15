// inner.js —— 第二个页面的行为（ES 模块 + 显式 .lui 导入）。
//
// 从 app.js 跳过来走的是 ui.navigate：同一个标签、记历史、地址栏跟着改。
// 这一页的文档是自己的 inner.lui —— 「页面长什么样」完全由页面自己的资源决定。

import doc from './inner.lui';

ui.render(doc);

ui.query('#btn-back').on('tap', () => {
  ui.navigate('/app.js');
});

setTimeout(() => {
  const r = ui.query('#btn-back').rect();
  console.log('rect #btn-back', Math.round(r.x), Math.round(r.y), Math.round(r.w), Math.round(r.h));
}, 200);

console.log('inner.js ready:', __page__.url);
