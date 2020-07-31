source("util.R")
set.seed(23)

method <- "quantile"
verbose <- TRUE

problist <- list(
                 0.0,
                 0.5,
                 0.99999,
                 c(0, 1/pi, 0.5, 0.9999, 1.0),
                 seq(0, 1, length.out=5),
                 seq(0, 1, length.out=100)
                 )

for (x in numeric_vectors(10, min_length=1)) {
    for (probs in problist) {
        if (verbose) {
            cat(paste("::::::: doing:", method, "\n"))
            cat(paste("x =", strify(x), "\n"))
            cat(paste("probs =", strify(probs), "\n"))
        }
        if(!compare_numeric(method, x, probs)) {
            stop(paste("Error!", method, "does not agree on", strify(x)))
        }
    }
}
