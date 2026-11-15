// app.js —— 页面的行为：ES 模块 + 组件类 + signal。
//
// 与旧版（拼字符串产出整份文档 + 手写 ui.set）的区别：
//   1. 界面资源显式导入：import doc from './app.lui' —— 没有「同目录同名自动配对」的隐式约定
//   2. 状态集中在 signal：改动状态即可，UI 由 effect 自动跟上（不整份 render、不手写更新循环）
//   3. 组件是类：template() 出结构（ui.append 挂进槽位），onMount() 绑行为，detach() 收尾
//
// 演示：计数 / 互斥选择 / 时钟 / 主题（类切换，不重渲染）/ 事件全序列 / 页面跳转。

import { signal, effect, computed, Component } from 'loppe';
import doc from './app.lui';

ui.render(doc);   // 静态结构一次到位；下面全是绑定，不再动结构

// ── 状态：唯一真相 ──
const count = signal(0);
const pick = signal('A');
const theme = signal('light');
const seconds = signal(0);
const evtText = signal('(事件日志)');
const themeNote = computed(() => '当前主题「' + theme.value + '」——换 class，不整份渲染。');

// ── 组件：一类一块，模板出结构、onMount 绑行为 ──

class Counter extends Component {
  template() {
    return `
      panel(class="card") {
        text(class="text", value="计数器：state 在 signal，effect 自动写 el.text。"),
        panel(class="row") {
          button(id="btn-add", class="btn") { text(class="btn-text", value="点我 +1") },
          text(id="count", class="count", value="0")
        }
      }`;
  }
  onMount() {
    ui.query('#btn-add').on('tap', () => { count.value += 1; console.log('tap +1 ->', count.value); });
    this.effect(() => { ui.query('#count').text = String(count.value); });
  }
}

class Chips extends Component {
  template() {
    return `
      panel(class="row") {
        text(class="text", value="互斥选择："),
        panel(id="chip-a", class="chip on") { text(id="chip-a-text", class="chip-on-text", value="A") },
        panel(id="chip-b", class="chip") { text(id="chip-b-text", class="text", value="B") }
      }`;
  }
  onMount() {
    ui.query('#chip-a').on('tap', () => { pick.value = 'A'; console.log('pick ->', pick.value); });
    ui.query('#chip-b').on('tap', () => { pick.value = 'B'; console.log('pick ->', pick.value); });
    this.effect(() => {
      const a = pick.value === 'A';
      ui.query('#chip-a').toggleClass('on', a);
      ui.query('#chip-b').toggleClass('on', !a);
    });
  }
}

class Clock extends Component {
  template() {
    return `
      panel(class="card") {
        text(id="clock", class="text", value="已运行 0 秒"),
        text(id="rect-out", class="text", value="(还没量)")
      }`;
  }
  onMount() {
    this.effect(() => { ui.query('#clock').text = '已运行 ' + seconds.value + ' 秒'; });
    setTimeout(() => this.tick(), 1000);
  }
  tick() {
    seconds.value += 1;
    const r = ui.query('#btn-add').rect();
    ui.query('#rect-out').text = 'btn-add @ ' + Math.round(r.x) + ',' + Math.round(r.y) +
                                '  ' + Math.round(r.w) + 'x' + Math.round(r.h);
    setTimeout(() => this.tick(), 1000);
  }
}

class ThemeCard extends Component {
  template() {
    return `
      panel(class="card") {
        text(id="theme-note", class="text", value=""),
        button(id="btn-theme", class="btn") { text(class="btn-text", value="切换主题") }
      }`;
  }
  onMount() {
    ui.query('#btn-theme').on('tap', () => {
      theme.value = theme.value === 'dark' ? 'light' : 'dark';
      console.log('theme ->', theme.value);
    });
    this.effect(() => {
      ui.query('#app').toggleClass('dark', theme.value === 'dark');
      ui.query('#theme-note').text = themeNote.value;
    });
  }
}

class EventLog extends Component {
  template() {
    return `
      panel(class="card") {
        text(class="text", value="事件全序列：点输入框打字（keydown/mods）、点别的（focus/blur）、右键点小圆片（contextmenu）。"),
        input(id="kbd", class="kbd-input", placeholder="点进来，按几个键（试试按住 Ctrl）"),
        panel(class="row") {
          panel(id="ctx-chip", class="chip") { text(class="text", value="右键点我") }
        },
        text(id="evt-out", class="text", value="(事件日志)")
      }`;
  }
  onMount() {
    const kbd = ui.query('#kbd');
    kbd.on('keydown', (e) => {
      const mods = [];
      if (e.mods & 1) mods.push('Shift');
      if (e.mods & 2) mods.push('Ctrl');
      if (e.mods & 4) mods.push('Alt');
      evtText.value = 'keydown key=' + e.key + (mods.length ? '  [' + mods.join('+') + ']' : '');
    });
    kbd.on('focus', () => { evtText.value = '输入框拿到焦点'; });
    kbd.on('blur', () => { evtText.value = '输入框失去焦点'; });
    ui.query('#ctx-chip').on('contextmenu', (e) => {
      evtText.value = 'contextmenu（右键）在 ' + e.targetId;
    });
    this.effect(() => { ui.query('#evt-out').text = evtText.value; });
  }
}

class NavCard extends Component {
  template() {
    return `
      panel(class="card") {
        text(class="text", value="ui.navigate：跳到同目录的 inner.js（它 import 自己的 inner.lui）。"),
        button(id="btn-nav", class="btn") { text(class="btn-text", value="去 inner.js") }
      }`;
  }
  onMount() {
    ui.query('#btn-nav').on('tap', () => {
      try {
        ui.navigate('/inner.js');
      } catch (err) {
        evtText.value = String(err);
      }
    });
  }
}

// ── 组装：组件当积木，各自挂进槽位 ──
new Counter().mount('#slot-counter');
new Chips().mount('#slot-chips');
new Clock().mount('#slot-clock');
new ThemeCard().mount('#slot-theme');
new EventLog().mount('#slot-events');
new NavCard().mount('#slot-nav');

// 自检：把关键控件的坐标打进控制台（回归验证用窗口消息点击就按这份来）。
// 等两拍：挂载后的布局要走完一帧才拿得到真实几何。
setTimeout(() => {
  try {
    console.log('rect-timer fired');
    for (const sel of ['#btn-add', '#chip-a', '#chip-b', '#btn-theme', '#kbd', '#ctx-chip', '#btn-nav']) {
      const r = ui.query(sel).rect();
      console.log('rect', sel, Math.round(r.x), Math.round(r.y), Math.round(r.w), Math.round(r.h));
    }
  } catch (err) {
    console.log('rect-timer error:', String(err));
  }
}, 200);
console.log('app.js ready:', __page__.url);
