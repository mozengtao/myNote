- 伪码是一种描述算法的结构化英语表达，目的是使得开发人员专注于算法本身而不用考虑具体的语言实现细节。
- 伪码所用的词汇应该是问题领域所用到的词汇，而不是实现领域的词汇，每个开发者可以有自己特定的伪码词汇。
-
- 常用操作关键字
	- 输入
		- READ, OBTAIN, GET
	- 输出
		- PRINT, DISPLAY, SHOW
	- 计算
		- COMPUTE, CALCULATE, DETERMINE
	- 初始化
		- SET, INIT
	- 自加
		- INCREMENT, BUMP
	- 示例
		- ```sh
		  READ height of rectangle
		  READ width of rectangle
		  COMPUTE area as height times width
		  ```
-
- 流程控制
	- IF-THEN-ELSE
		- ```sh
		  IF condition THEN
		  	sequence 1
		  ELSE
		  	sequence 2
		  ENDIF
		  
		  # Example
		  IF HoursWorked > NormalMax THEN
		  	DISPLAY overtime message
		  ELSE
		  	DISPLAY regular time message
		  ENDIF
		  ```
	- WHILE
		- ```sh
		  WHILE condition
		  	sequence
		  ENDWHILE
		  
		  # Example
		  WHILE Population < Limit
		  	Compute Population AS Population + Births - Deaths
		  ENDWHILE
		  
		  WHILE employee.type NOT EQUAL manager AND personCount < numEmployees
		  	INCREMENT personCount
		  	CALL employeList.getPerson WITH personCount RETURNING employee
		  ENDWHILE
		  ```
	- CASE
		- ```sh
		  CASE expression OF
		  	condition 1: sequence 1
		      condition 2: sequence 2
		      ...
		      condition n: sequence n
		      OTHERS:		 default sequence
		  ENDCASE
		  
		  # Example
		  CASE Title OF
		  	Mr		: Print "Mister"
		      Mrs		: Print "Missus"
		      Miss	: Print "Miss"
		      Ms		: Print "Mizz"
		      Dr		: Print "Doctor"
		  ENDCASE
		  
		  CASE grade OF
		  	A	: points = 4
		      B	: points = 3
		      C	: points = 2
		      D	: points = 1
		      E	: points = 0
		  ENDCASE
		  ```
	- REPEAT-UNTIL
		- ```sh
		  REPEAT
		  	sequence
		  UNTIL condition
		  ```
	- FOR
		- ```sh
		  FOR iteration bounds
		  	sequence
		  ENDFOR
		  
		  # Example
		  FOR each month of the year
		  FOR each employee in the list
		  ```
	- NESTED CONTRUCTS
		- ```sh
		  SET total to zero
		  REPEAT
		  	READ Temperature
		      IF Temperature > Freezing THEN
		      	INCREMENT total
		      ENDIF
		  UNTIL Temperature < zero
		  PRINT total
		  ```
	- INVOKING SUBPROCEDURES
		- ```sh
		  CALL AvgAge WITH StudentAges
		  CALL Swap WITH CurrentItem and TargetItem
		  CALL Account.debit WITH CheckAmount
		  CALL getBalance RETURNING eBalance
		  CALL SquareRoot WITH orbitHeight RETURNING nominalOrbit
		  ```
	- EXCEPTION HANDLING
		- ```sh
		  BEGIN
		  	statements
		  EXCEPTION
		  	WHEN exception type
		      	statements to handle exception
		      WHEN another exception type
		      	statements to handle exception
		  END
		  ```
-
- 完整示例
	- ```sh
	  SET moveCount to 1
	  FOR each row on the board
	  	FOR each column on the board
	      	IF gameBoard position (row, column) is occupied THEN
	          	CALL findAdjacentTiles WITH row, column
	              INCREMENT moveCount
	          ENDIF
	      ENDFOR
	  ENDFOR
	  ```
	- ```sh
	  SET Carry to 0
	  FOR each DigitPosition in Number from least significant to most significant
	  	
	      COMPUTE Total as sum of FirstNum[DigitPositon] and SecondNum[DigitPosition] and Carry
	      
	      IF Total > 10 THEN
	      	SET Carry to 1
	          SUBTRACT 10 from Total
	      ELSE
	      	SET Carry to 0
	      ENDIF
	      
	      STORE Total in Result[DigitPosition]
	  ENDFOR
	  
	  IF Carry = 1 THEN
	  	RAISE Overflow exception
	  ENDIF
	  ```
-
-
- 参考文档
	- [PSEUDOCODE STANDARD](https://users.csc.calpoly.edu/~jdalbey/SWE/pdl_std.html)
	-