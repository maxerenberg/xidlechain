" For ALE users
let g:ale_cpp_cc_options .= " " . trim(system('pkg-config --cflags gdk-x11-3.0 gio-unix-2.0 gudev-1.0 xext libpulse libpulse-mainloop-glib'))
