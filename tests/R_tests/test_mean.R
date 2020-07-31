source("util.R")
set.seed(23)

method <- "mean"
verbose <- FALSE

for (x in numeric_vectors(10, min_length=1)) {
    if (verbose) {
        cat("::::::: doing: \n")
        cat(strify(x))
        cat("\n")
    }
    if(!compare_numeric(method, x)) {
        stop(paste("Error!", method, "does not agree on", strify(x)))
    }
}
