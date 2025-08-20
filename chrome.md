## 插件
### 代理
- SwitchyOmega
- WonderSwitcher
- DeepL翻译
- Weava Highlighter
- Octotree
- Octoree Theme
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