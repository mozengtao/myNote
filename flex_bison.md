> Lexical Analysis with Flex: Split input data into a set of tokens (identifiers, keywords, numbers, brackets, braces, etc.)
> Semantic Parsing with Bison: Generate an AST while parsing the tokens. Bison will do most of the legwork here, we just need to define our AST.
> Assembly with LLVM: This is where we walk over our AST and generate byte/machine code for each node. As crazy as it sounds, this is probably the easiest step.

## Flex
- 3 sections
```
Definitions section
%%
Rules section
%%
User code section
```

## Bison
- 4 main sections
```
%{
Prologue section
%}
Declarations section
%% 
Grammar rules section
%%
Epilogue section
```

[Practical parsing with Flex and Bison](https://begriffs.com/posts/2021-11-28-practical-parsing.html)  
[What are Flex and Bison?](http://aquamentus.com/flex_bison.html)  
[GNU Bison](https://www.gnu.org/software/bison/manual/)  
[Writing Your Own Toy Compiler Using Flex, Bison and LLVM](https://gnuu.org/2009/09/18/writing-your-own-toy-compiler/)  
[Flex Bison](https://www.skenz.it/compilers/flex_bison)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  