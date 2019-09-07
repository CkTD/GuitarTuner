guitartuner_sources = [
  'main.c',
  'guitartuner-window.c',
  'guitartuner-app.c',
  'guitartuner-freq.c',
  'guitartuner-note.c',
  'guitartuner-prefs.c',
]


guitartuner_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('gstreamer-1.0', version: '>=1.16'),
  meson.get_compiler('c').find_library('m', required: true),
]

gnome = import('gnome')

guitartuner_sources += gnome.compile_resources('guitartuner-resources',
  'guitartuner.gresource.xml',
  c_name: 'guitartuner'
)


executable('guitartuner', guitartuner_sources,
  dependencies: guitartuner_deps,
  install: true,
)


