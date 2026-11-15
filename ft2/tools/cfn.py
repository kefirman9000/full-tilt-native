# cfn.py C ADDR...: print functions from an annc.py-labelled dump without typedef noise
# fn.py C ADDR... : print functions from the annotated dump without typedef noise
import sys, re
c = open(sys.argv[1]).read()
parts = re.split(r'^// ===== ', c, flags=re.M)
want = [a.lower() for a in sys.argv[2:]]
for p in parts:
    head = p.split('\n', 1)[0]
    if head.split() and head.split()[0] in want:
        body = re.sub(r'typedef struct [^}]*\} [^;]*;\n\n?', '', p.split('\n', 1)[1], flags=re.S)
        body = re.sub(r'^extern .*\n', '', body, flags=re.M)
        body = re.sub(r'^\s+\w[\w ]*\*? *v\d+;  // .*\n', '', body, flags=re.M)
        print('// =====', head); print(body.strip()); print()
