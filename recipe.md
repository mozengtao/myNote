- BitBake Recipes, which are denoted by the file extension .bb, are the most basic metadata files.
	- Descriptive information about the package (author, homepage, license, and so on)
	- The version of the recipe
	- Existing dependencies (both build and runtime dependencies)
	- Where the source code resides and how to fetch it
	- Whether the source code requires any patches, where to find them, and how to apply them
	- How to configure and compile the source code
	- How to assemble the generated artifacts into one or more installable packages
	- Where on the target machine to install the package or packages created