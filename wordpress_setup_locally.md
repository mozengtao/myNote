# WordPress 本地开发环境搭建指南 (新手程序员版)

在本地电脑搭建环境是学习 WordPress 最明智的选择。它免费、快速，且允许你在不影响线上用户的情况下随意“折腾”代码。

---

## 方案一：使用 LocalWP (最推荐，5分钟上手)
这是目前开发者公认最优雅、最简单的工具，专为 WordPress 优化，自带域名模拟和数据库管理。

### 搭建步骤：
1. **下载：** 前往 [localwp.com](https://localwp.com) 下载对应操作系统的安装包。
2. **创建：** 点击左下角的 **"+"** 号，选择 "Create a new site"，输入你的网站名字（如 `mytestsite`）。
3. **配置：** 选择 **"Preferred"** 配置（它会自动匹配最稳定的 PHP 和 MySQL 版本）。
4. **设置账号：** 设置你的 **WP Admin** 用户名和密码（请务必牢记）。
5. **启动：** - 点击 **"Open Site"** 访问前端网页。
   - 点击 **"WP Admin"** 进入管理后台。

**💡 优点：** 自动处理 SSL 证书和本地 `hosts` 文件，支持一键切换 PHP 版本，界面极简。

---

## 方案二：使用 XAMPP (程序员经典款)
如果你想学习通用型服务器环境配置（Apache/MySQL），XAMPP 是经典选择。

### 搭建步骤：
1. **安装：** 下载并安装 [XAMPP](https://www.apachefriends.org/)，在控制面板启动 **Apache** 和 **MySQL**。
2. **下载源码：** 去 [cn.wordpress.org](https://cn.wordpress.org/download/) 下载最新的 WordPress `.zip` 包并解压。
3. **放置文件：** 将解压后的 `wordpress` 文件夹整体移动到 XAMPP 安装目录下的 `htdocs` 文件夹中。
4. **创建数据库：** 浏览器访问 `http://localhost/phpmyadmin`，新建一个名为 `wordpress_db` 的数据库。
5. **运行安装：** 浏览器访问 `http://localhost/wordpress`：
   - 数据库名：`wordpress_db`
   - 用户名：`root`
   - 密码：(留空)
   - 数据库主机：`localhost`

---

## 🚀 练习时的三个“实验室”任务

当你进入 WordPress 后台后，按顺序尝试以下操作来理解其逻辑：

### 1. 搞定“增删改查” (CRUD)
* **文章 (Posts)：** 写一篇博客，上传一张图片。
* **页面 (Pages)：** 创建一个“关于我”页面。
* **思考：** 观察两者的 URL 结构有什么不同？（提示：文章通常有分类和日期属性）。

### 2. 体验“换装” (Theming)
* 进入 **“外观” -> “主题”**，搜索并安装 **Astra** 或 **GeneratePress** 主题。
* 点击 **“自定义” (Customize)**，尝试修改网站顶部的 Logo 或主题颜色，观察实时预览。

### 3. 查看数据库 (进阶)
* **LocalWP 用户：** 点击 "Database" -> "Open Adminer"。
* **任务：** 找到 `wp_posts` 这张表。你会发现你刚才写的文章、页面、甚至导航菜单，全部都存在这张表里。这就是“内容与样式分离”的直观证据。

---

## ⚠️ 避坑小贴士 (Key Points)

* **本地不联网：** 你的本地网站只有你自己能看到。如果未来想发布到公网，需要使用插件（如 All-in-One WP Migration）将数据迁移到云服务器。
* **代码主战场：** 记住所有的主题和插件代码都存在 `/wp-content/` 目录下。不要动 `/wp-admin` 或 `/wp-includes` 里的文件。
* **开启 Debug：** 如果遇到白屏或报错，打开根目录下的 `wp-config.php`，将 `define( 'WP_DEBUG', false );` 改为 `true`，你会看到详细的报错信息。