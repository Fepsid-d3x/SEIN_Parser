import sein

doc = sein.create_new_config("./configPY.sein")

sein.add_header_comment(doc, "Auto-generated")
sein.add_include(doc, "colors.sein")
sein.add_global_var(doc, "APP_NAME", "My App")
sein.add_section(doc, "Window")
sein.add_value(doc, "Window", "Title", "${APP_NAME}")
sein.add_value(doc, "Window", "Width", "1280", "px")
sein.save_config(doc)