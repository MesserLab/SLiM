###
# Things for constructing eidos tests

# change this to the absolute path of a different binary to test with that one
EIDOS <- "eidos"

eidos_run <- function(method, ...
                      ) {
    command <- eidos_command(method, ...)
    f <- tempfile()
    cat(command, file=f)
    out <- system(paste(EIDOS, f), intern=TRUE)
    return(out)
}

eidos_command <- function (method, ...) {
    args <- list(...)
    str_args <- lapply(args, strify)
    command <- paste0(method, "(", paste(str_args, collapse=', '), ");")
    return(command)
}

strify <- function (x, digits=5) {
    # probably needs to be extended to more types
    if (is.vector(x) && length(x) > 1) {
        str_vector(x)
    } else if (is.integer(x)) {
        sprintf("%d", x)
    } else if (is.numeric(x)) {
        sprintf(paste0("%0.", digits, "f"), x)
    } else {
        paste(x)
    }
}

str_vector <- function (x) {
    # Turn a vector into a string, like "c(1, 2, 3)" or "c('a', 'b', 'c')"
    x <- as.vector(x)
    if (is.character(x)) {
        x <- paste("'", x, "'", sep='')
    }
    out <- paste0("c(", paste(x, collapse=', '), ")", sep='')
    return(out)
}


###
# test for agreement between eidos output and R's answer

compare_numeric <- function (method, ...,
                             digits=5,
                             verbose=TRUE
                             ) {
    e_ans <- eidos_run(method, ...)
    e_ans <- as.numeric(e_ans)
    r_ans <- do.call(method, list(...))
    r_ans <- round(r_ans, digits=digits)
    result <- all(isTRUE(all.equal(e_ans, r_ans, check.attributes=FALSE)))
    if (!result) {
        cat("-------------\n")
        cat("This:\n")
        dput(e_ans)
        cat("is not equal to this:\n")
        dput(r_ans)
        cat("-------------\n")
        stop('here')
    }
    return(result)
}

###
# test generation utilities

numeric_vectors <- function (n, min_length=0, max_length=Inf, seed=5, digits=5) {
    # Returns n vectors of length between min_length and max_length
    set.seed(5)
    out <- list(
                c(),
                1:10,
                rep(0, 5),
                pi + 1:11)
    out <- out[sapply(out, length) >= min_length & sapply(out, length) <= max_length]
    rfns <- list(runif, rexp, rnorm, rcauchy)
    while (length(out) < n) {
        arglen <- min(max_length, min_length + rpois(1, lambda=100))
        out <- c(out,
                 list(do.call(sample(rfns, 1)[[1]], list(n=arglen)))
                 )
    }
    return(lapply(out, round, digits=digits))
}
