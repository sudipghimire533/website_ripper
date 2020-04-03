# Rip_Website
This is the simpele Hard-Code { changes can ony be done by editing source } program to download all the associated files and everything to the local drive preserving the orginal directory structure.
# Algorithm
1) Get the main page
2) Prase the page and extract all links
3) Store liks in catagory as images[], medias[], web_file[]
4) WebFile[] is the browser-renderable files i.e html, php, asp and so on
5) Loop the same process for all WebFile[]
6) Get the final arary of all catagory
7) Download with minimal and simple get request from curl
8) Save The content to desired output path
# Building Deps:
  1) c++17 compatiable compiler
  2) libcurl
  3) Guts to repair if problem occurs
# ToDos:
  1) Fix if any download is missing
  2) Make Portable and tidy
  3) Possibly add a Gui with capablity to render web using qt/qml modules and libs
  4) Make Suprisingly beter
