project('x3d-toggle-docs', 'c', version: '2.1.0')

pandoc = find_program('pandoc', required: false)

if pandoc.found()
  custom_target('manpage',
    input: 'docs/x3d-toggle.1.md',
    output: 'x3d-toggle.1',
    command: [pandoc, '-s', '-t', 'man', '@INPUT@', '-o', '@OUTPUT@'],
    build_by_default: true,
    install: true,
    install_dir: get_option('mandir') / 'man1'
  )
else
  warning('pandoc not found. Cannot generate x3d-toggle.1 from Markdown.')
endif
