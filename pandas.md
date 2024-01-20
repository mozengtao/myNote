- 在 python 中，pandas是一个进行数据分析和处理的 Python 库
-
- pandas 的主要应用
	- quick Exploratory Data Analysis (EDA)
	- drawing attractive plots
	- feeding data into machine learning tools like scikit-learn
	- building machine learning models on your data
	- taking cleaned and processed data to any number of data tools
-
- 处理 Excel 依赖的 Python 库
	- [matplotlib](https://matplotlib.org/) – data visualization
	- [NumPy](https://www.numpy.org/) – numerical data functionality
	- [OpenPyXL](https://openpyxl.readthedocs.io/en/stable/) – read/write Excel 2010 xlsx/xlsm files
	- [pandas](https://pandas.pydata.org/) – data import, clean-up, exploration, and analysis
	- [xlrd](https://xlrd.readthedocs.io/en/latest/) – read Excel data
	- [xlwt](https://xlwt.readthedocs.io/en/latest/) – write to Excel
	- [XlsxWriter](https://xlsxwriter.readthedocs.io/) – write to Excel (xlsx) files
-
- 处理Excel文件的示例
	- [Tutorial Using Excel with Python and Pandas](https://www.dataquest.io/blog/excel-and-pandas/)
		- ```python
		  # import pandas module for later use
		  import pandas as pd
		  import matplotlib.pyplot as plt
		  
		  excel_file = 'movies.xls'
		  # 读取 Excel 文件并保存在 pandas DataFrame 对象
		  movies = pd.read_excel(excel_file)
		  # 显示 DataFrame 对象的前几列
		  print(movies.head())
		  
		  # 通过指定 index_col = 0 来选择'Title'列作为索引0
		  movies_sheet1 = pd.read_excel(excel_file, sheet_name=0, index_col=0)
		  print(movies_sheet1.head())
		  
		  # 读取Excel文件内的表单2
		  movies_sheet2 = pd.read_excel(excel_file, sheet_name=1, index_col=0)
		  print(movies_sheet2.head())
		  
		  # 读取Excel文件内的表单3
		  movies_sheet3 = pd.read_excel(excel_file, sheet_name=2, index_col=0)
		  print(movies_sheet3.head())
		  
		  # 拼接多个 DataFrame 对象到一起作为一个DataFrame对象
		  movies = pd.concat([movies_sheet1, movies_sheet2, movies_sheet3])
		  # 检查拼接结果 (rows, columns)
		  print(movies.shape)
		  
		  # 使用ExcelFile 类来读取多个表单，如果一个 Excel文件包含许多 sheets 的情况，这时可以显著提高处理性能
		  xlsx = pd.ExcelFile(excel_file)
		  movies_sheets = []
		  for sheet in xlsx.sheet_names:
		  	movies_sheets.append(xlsx.parse(sheet))
		  movies = pd.concat(movies_sheets)
		  
		  # 数据挖掘
		  # 显示 DataFrame 的行数和列数
		  print(movies.shape)
		  # 显示倒数若干行的数据
		  print(movies.tail())
		  
		  # 对excel文件中的表进行排序
		  sorted_by_gross = movies.sort_values(['Gross Earnings'], ascending=False)
		  # 根据 Gross Earnings 显示前10条数据
		  print(sorted_by_gross['Gross Earnings'].head(10))
		  
		  # 图表显示结果
		  sorted_by_gross['Gross Earnings'].head(10).plot(kind="barh")
		  # 图表显示依赖 GUI backend，需要通过终端命令 pip install pyqt5 解决
		  plt.show()
		  
		  # 柱状图
		  movies['IMDB Score'].plot(kind="hist")
		  plt.show()
		  
		  #获取数据的统计信息
		  # describe 方法用来获取每一列的如下数字统计信息
		  #the count or number of values
		  #mean
		  #standard deviation
		  #minimum, maximum
		  #25%, 50%, and 75% quantile
		  print(movies.describe())
		  
		  # 获取某一列的平均值信息
		  print(movies['Gross Earnings'].mean())
		  
		  # 在一些情况下，Excel文件没有 header，或者前几行不是真正的数据，因此读取的时候需要跳过前几行
		  movies_skip_rows = pd.read_excel(excel_file, header=None, skiprows=4)
		  print(movies_skip_rows.head(5))
		  
		  # 跳过倒数几行
		  movies_skip_rows = pd.read_excel(excel_file, header=None, skipfooter=4)
		  print(movies_skip_rows.head(5))
		  
		  # 重新指定列名称
		  movies_skip_rows.columns = ['Title', 'Year', 'Genres', 'Language', 'Country', 'Content Rating', 'Duration', 'Aspect Ratio', 'Budget', 'Gross Earnings', 'Director', 'Actor 1', 'Actor 2', 'Actor 3', 'Facebook Likes - Director', 'Facebook Likes - Actor 1', 'Facebook Likes - Actor 2', 'Facebook Likes - Actor 3', 'Facebook Likes - cast Total', 'Facebook likes - Movie', 'Facenumber in posters', 'User Votes', 'Reviews by Users', 'Reviews by Crtiics', 'IMDB Score']
		  print(movies_skip_rows.head(10))
		  
		  # 读取某些列的内容
		  # 只读取前6列的内容
		  movies_subnet_columns = pd.read_excel(excel_file, usecols=range(6))
		  print(movies_subnet_columns.head())
		  
		  # 列公式
		  movies["Net Earnings"] = movies["Gross Earnings"] - movies["Budget"]
		  sorted_movies = movies[['Net Earnings']].sort_values(['Net Earnings'], ascending=[False])
		  sorted_movies.head(10)['Net Earnings'].plot.barh()
		  plt.show()
		  
		  
		  # 数据透视表
		  movies_subnet = movies[['Year', 'Gross Earnings']]
		  print(movies_subnet.head())
		  earnings_by_year = movies_subnet.pivot_table(index=['Year'])
		  earnings_by_year.plot()
		  plt.show()
		  
		  movies_subset = movies[['Country', 'Language', 'Gross Earnings']]
		  print(movies_subset.head())
		  earnings_by_co_lang = movies_subset.pivot_table(index=['Country', 'Language'])
		  print(earnings_by_co_lang.head())
		  earnings_by_co_lang.head(20).plot(kind='bar', figsize=(20,8))
		  plt.show()
		  
		  # 将结果导出到Excel文件
		  movies.to_excel('output.xlsx')
		  # 不保存index到文件
		  movies.to_excel('output2.xlsx', index=False)
		  
		  # 利用 pandas的ExcelWriter类和XlsxWriter 模块格式化输出
		  writer = pd.ExcelWriter('output3.xlsx', engine='xlsxwriter')
		  movies.to_excel(writer, index=False, sheet_name='report')
		  workbook = writer.bookworksheet = writer.sheets['report']
		  
		  #header_fmt = workbook.add_format({'bold': True})
		  #worksheet.set_row(0, None, header_fmt)
		  
		  writer.save()
		  
		  # 同样也可以使用XlsxWriter 应用不同的输出格式
		  ```
-
- 帮助文档
	- [pandas处理Excel示例](https://sparkbyexamples.com/pandas/pandas-read-excel-with-examples/)
	- [pandas用户手册](https://pandas.pydata.org/docs/user_guide/index.html)