## 插件
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