# Keywords: Python, tree-sequence recording, tree sequence recording

# This is a Python recipe; note that it runs the SLiM model internally, below

import subprocess, tskit
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import tspop

# Run the SLiM model and load the resulting .trees file
subprocess.check_output(["slim", "-m", "-s", "0", "./admix.slim"])
ts = tskit.load("./admix.trees")

# Assign ancestry
root_time = max(ts.node(n).time for n in ts.samples())
pa = tspop.get_pop_ancestry(ts, census_time=root_time)
st = pa.squashed_table

stepfun = pd.melt(st[st.population==2], value_vars=['left', 'right'], var_name='side', value_name='pos')
stepfun['dy'] = [1 if s == 'left' else -1 for s in stepfun['side']]
stepfun.sort_values("pos", inplace=True)
stepfun['y'] = np.cumsum(stepfun['dy']) / pa.coverage
stepfun = stepfun[stepfun.pos > 0]
stepfun = stepfun[stepfun.pos < ts.sequence_length]

# Make a simple plot
plt.plot(stepfun['pos'], stepfun['y'])
plt.show()
