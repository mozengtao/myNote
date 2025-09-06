## 插件
- Text Blaze
- Web Highlights
- Weava Highlighter
- SwitchyOmega
- WonderSwitcher
- DeepL翻译
- Octotree
- Octoree Theme
- OneTab
- Chrome Remote Desktop
- Bookmarks Quick Search
- Momentum
- Todoist
- Video DownloadHelper
- Sider
- DeepL：人工智能翻译器和写作助手
- Mate Translate
- DeepTranslate
- Session Buddy
- Adblock Plus
- Auto Refresh Plus | Page Monitor
- Chrono下载管理器
- 网络绘画 - 页标记和编辑器
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