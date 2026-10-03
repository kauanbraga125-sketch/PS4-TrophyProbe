from pathlib import Path
import sys

path = Path(sys.argv[1] if len(sys.argv) > 1 else "pkg.gp4")
text = path.read_text()

needle = '		<dir targ_name="sce_sys">\n			<dir targ_name="about" />\n		</dir>'
replacement = '		<dir targ_name="sce_sys">\n			<dir targ_name="about" />\n			<dir targ_name="trophy" />\n		</dir>'

if '<dir targ_name="trophy" />' in text:
    raise SystemExit(0)
if needle not in text:
    raise SystemExit("sce_sys rootdir anchor not found in generated GP4")

path.write_text(text.replace(needle, replacement, 1))
print("Added sce_sys/trophy to GP4 rootdir")
