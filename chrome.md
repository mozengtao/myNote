## 插件
- iTab新标签页
- OmniTab 新标签页
- Extension Manager

- Toby: Tab Management Tool
  / for quick search
- Tabme (新标签页书签和标签管理器)
- Omni - Bookmark, History, & Tab Manager
- Simple Tab Manager
- Tab Modifier
- Rename Tab Title
- OneTab
- Session Buddy
[Toby Alternatives](https://www.producthunt.com/products/toby/alternatives)  

- Dark Reader

- Simple Proxy Switcher
- WonderSwitcher
- SwitchyOmega
- Proxy SwitchyOmega Pro
- Zero Omega
- 
- 
- 
- 
- 
- 
- Text Blaze

- Web Highlights
- Weava Highlighter

- DeepL翻译
- Octotree
- Octoree Theme

- Bookmarks(书签侧边栏)
- Bookmarks Quick Search
- Better Bookmarks (简化您的收藏夹)

- Momentum
- Todoist
- Video DownloadHelper
- Sider

- DeepL：人工智能翻译器和写作助手
- Mate Translate
- DeepTranslate

- Adblock Plus
- Auto Refresh Plus | Page Monitor
- Chrono Download Manager
- 网络绘画 - 页标记和编辑器
- 
- Note Board - Sticky Notes App

- Bulk URL Opener Extension
- Copy All URLs (Free)

- Scientific Calculator
- 
- 

### Tips
按键`/`可以直接跳转至搜索框


[The Ultimate Guide to Bookmarklets](https://www.bookmarkllama.com/blog/bookmarklets)  
[What are Bookmarklets?](https://www.freecodecamp.org/news/what-are-bookmarklets/)  
[]()  
[]()  
[]()  
[]()  
[]()  


```javascript
// 控制台执行
document.designMode = "on";

// Bookmarklet: Highlight Text
javascript:(function(){
  var sel=window.getSelection();
  if(sel.rangeCount){
    var range=sel.getRangeAt(0);
    var span=document.createElement('span');
    span.style.backgroundColor='yellow';
    span.style.color='black';
    range.surroundContents(span);
  }
})();

```