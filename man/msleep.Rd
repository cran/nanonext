% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/utils.R
\name{msleep}
\alias{msleep}
\title{Sleep Utility}
\usage{
msleep(msec)
}
\arguments{
\item{msec}{integer number of milliseconds to block the caller.}
}
\value{
Invisible NULL.
}
\description{
Sleep function. May block for longer than requested, with the actual wait
    time determined by the capabilities of the underlying system.
}
\examples{
time <- mclock(); msleep(100); mclock() - time

}