import collections
import re
import sys

txt = open(sys.argv[1], encoding='utf-8', errors='replace').read()
limit = int(sys.argv[2]) if len(sys.argv) > 2 else 40
print(re.findall(r"\*\* Found .*", txt))
c = collections.Counter()
rows = []
for b in re.split(r'\n(?=\[)', txt):
    if not b.startswith('['):
        continue
    lines = [l.strip() for l in b.strip().split('\n') if not l.startswith('**')]
    kind = lines[0].split(']')[0][1:]
    c[kind] += 1
    items = [re.sub(r'@\(([\d.]+) mm, ([\d.]+) mm\): ', r'(\1,\2) ', l) for l in lines[2:4]]
    rows.append((kind, lines[0].split(': ', 1)[1][:70] + ' | ' + ' | '.join(items)))
print(dict(c))
seen = collections.Counter()
for k, r in rows:
    seen[k] += 1
    if seen[k] <= limit:
        print('[%s] %s' % (k, r[:300]))
