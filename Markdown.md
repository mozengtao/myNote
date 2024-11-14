[Markdown基本语法](https://www.markdown.xyz/basic-syntax/)  
[Markdown Basic Syntax](https://www.markdownguide.org/basic-syntax/)  

### Headings
```
# Heading level 1
## Heading level 2
### Heading level 3
#### Heading level 4
##### Heading level 5
###### Heading level 6
```

### Headings Alternate Syntax
```
Heading level 1
=====

Heading level 2
---
```

### Paragraphs
```
use a blank line to separate one or more lines of text.

line 1

line 2
```

### Line Breaks
```
To create a line break or new line (<br>), end a line with two or more spaces, and then type return.

line1   (here there are more that 2 spaces)
line 2
```

### Emphasis
#### Bold
```
I just love **bold text**.

Love**is**bold
```
#### Italic
```
Italicized text is the *cat's meow*.

A*cat*meow
```

#### Bold and Italic
```
This text is ***really important***.

This is really***very***important text.
```

### Blockquotes
```
For compatibility, put blank lines before and after blockquotes

> Dorothy followed her through many of the beautiful rooms in her castle.
```

#### Blockquotes with Multiple Paragraphs
```
Blockquotes can contain multiple paragraphs. Add a > on the blank lines between the paragraphs.

> Dorothy followed her through many of the beautiful rooms in her castle.
>
> The Witch bade her clean the pots and kettles and sweep the floor and keep the fire fed with wood.
```

#### Nested Blockquotes
```
Blockquotes can be nested. Add a >> in front of the paragraph you want to nest

> Dorothy followed her through many of the beautiful rooms in her castle.
>
>> The Witch bade her clean the pots and kettles and sweep the floor and keep the fire fed with wood.
```

#### Blockquotes with Other Elements
```
Blockquotes can contain other Markdown formatted elements. Not all elements can be used — you’ll need to experiment to see which ones work.

> #### The quarterly results look great!
>
> - Revenue was off the chart.
> - Profits were higher than ever.
>
>  *Everything* is going according to **plan**.
```

### Lists
#### Ordered Lists
```
To create an ordered list, add line items with numbers followed by periods. The numbers don’t have to be in numerical order, but the list should start with the number one.

1. Frist item
1. Second item
1. Third item
1. Fourth item

1. Frist item
2. Second item
3. Third item
4. Fourth item

1. Frist item
8. Second item
4. Third item
4. Fourth item

1. Frist item
2. Second item
3. Third item
4. Fourth item

1. Frist item
2. Second item
3. Third item
     1. Indented item
     2. Indented item
4. Fourth item
```

#### Unordered Lists
```
To create an unordered list, add dashes (-), asterisks (*), or plus signs (+) in front of line items. Indent one or more items to create a nested list

- Frist item
- Second item
- Third item
- Fourth item


- Frist item
- Second item
- Third item
     - Indented item
     - Indented item
- Fourth item
```

#### Starting Unordered List Items With Numbers
```
If you need to start an unordered list item with a number followed by a period, you can use a backslash (\) to escape the period.

- 1968\. A great year!
- I think 1969 was second best.
```

#### Adding Elements in Lists
```
To add another element in a list while preserving the continuity of the list, indent the element four spaces or one tab, as shown in the following examples.

// Paragraphs

* This is the first list item.
* Here's the second list item.

    I need to add another paragraph below the second list item.

* And here's the third list item.

// Blockquotes

* This is the first list item.
* Here's the second list item.

    > A blockquote would look great below the second list item.

* And here's the third list item.

// Code Blocks
Code blocks are normally indented four spaces or one tab. When they’re in a list, indent them eight spaces or two tabs.

1. Open the file.
2. Find the following code block on line 21:

        <html>
          <head>
            <title>Test</title>
          </head>

3. Update the title to match the name of your website.

// Images

1. Open the file containing the Linux mascot.
2. Marvel at its beauty.

    ![Tux, the Linux mascot](/assets/images/tux.png)

3. Close the file.

// Lists
You can nest an unordered list in an ordered list, or vice versa.

1. First item
2. Second item
3. Third item
    - Indented item
    - Indented item
4. Fourth item
```

### Code
```
To denote a word or phrase as code, enclose it in backticks (`)

At the command prompt, type `nano`.
```

### Escaping Backticks
```
If the word or phrase you want to denote as code includes one or more backticks, you can escape it by enclosing the word or phrase in double backticks (``).

``Use `code` in your Markdown file.``
```

### Code Blocks
```
To create code blocks, indent every line of the block by at least four spaces or one tab.

    <html>
      <head>
      </head>
    </html>
```

### Horizontal Rules
```
To create a horizontal rule, use three or more asterisks (***), dashes (---), or underscores (___) on a line by themselves.

For compatibility, put blank lines before and after horizontal rules.

---

---
```

### Links
```
To create a link, enclose the link text in brackets (e.g., [Duck Duck Go]) and then follow it immediately with the URL in parentheses (e.g., (https://duckduckgo.com)).

My favorite search engine is [Duck Duck Go](https://duckduckgo.com).

// Adding Titles

ou can optionally add a title for a link. This will appear as a tooltip when the user hovers over the link. To add a title, enclose it in quotation marks after the URL.

My favorite search engine is [Duck Duck Go](https://duckduckgo.com "The best search engine for privacy").
```

### URLs and Email Addresses
```
To quickly turn a URL or email address into a link, enclose it in angle brackets.

<https://www.markdownguide.org>
<fake@example.com>
```