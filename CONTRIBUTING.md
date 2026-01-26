# Contributing to SLiM

Welcome!  This document outlines some guidelines and procedures for participating in and contributing to the SLiM community.  You can read about SLiM at the [<u>SLiM home page</u>](https://messerlab.org/slim/).


## Code of conduct

Be polite, kind, and considerate.  Do not spam, advertise, or proselytize.

**AI-generated contributions violate the code of conduct.**  An exception is made for the use of AI to translate text from another language.


## Getting started with SLiM

If you're a **new user wondering how to get started**, the [<u>SLiM home page</u>](https://messerlab.org/slim/) is a good place to start.  It provides a quick overview of what SLiM is, with links to a couple of podcast episodes in which SLiM was discussed.  Downloadable manuals for Eidos and SLiM are available there.

It also has a section about the **SLiM Workshop**, a tutorial process we have developed for new users.  The [<u>SLiM Workshop</u>](https://messerlab.org/slim/#Workshops) is probably the best way to get into SLiM; you can register to take it in person when it is offered, or download the free online materials for the workshop and do it on your own time.  The in-person workshops generally fill up quickly, so you would want to register for them soon after they are posted.  You can see announcements of new workshops on the [<u>slim-discuss list</u>](https://groups.google.com/g/slim-discuss), or the [<u>slim-announce list</u>](https://groups.google.com/g/slim-announce) if you only want to see announcements (not questions).  **If you wish to join slim-discuss, you must use an institutional email address (.edu, .gov, etc.), state your institution, and supply a SLiM-related "reason for joining" in your request.**


## Bug reports and feature requests

If you think you're seeing **a bug in SLiM or Eidos**, or you have a **feature request for SLiM or Eidos**, please open a new issue on [<u>SLiM's GitHub repository</u>](https://github.com/MesserLab/SLiM/issues).  For **bugs or feature requests pyslim**, please open an issue in the [<u>pyslim repository</u>](https://github.com/tskit-dev/pyslim/issues).  Please first search for similar issues, to save yourself and others time.  For bugs in other parts of the software ecosystem, such as msprime or tskit, please use a search engine to find the correct process; please do *not* ask us for support for that software.

Writing up **a good bug report** is an art form, and it is greatly appreciated by us, the developers, on the other end – and it makes it much more likely that your bug will actually get fixed.  For further guidance on writing a good bug report, please consult the paper [<u>Ten simple rules on reporting a bug</u>](https://journals.plos.org/ploscompbiol/article?id=10.1371/journal.pcbi.1010540).  Particularly important is constructing a **minimal reproducible example**; without that, it is often difficult for us to do much with your bug report. If you're not sure what a minimal reproducible example is, please read that paper.


## Asking a question

If you want to ask a question about SLiM, Eidos, or pyslim, please do not file an issue on GitHub.  Instead, join the [<u>slim-discuss list</u>](https://groups.google.com/g/slim-discuss) and ask it there.  Please search first to see whether others have already asked the same question.  Note that slim-discuss is frequently targeted by spammers, and is therefore heavily moderated.  **If you wish to join slim-discuss, you must use an institutional email address (.edu, .gov, etc.), state your institution, and supply a SLiM-related "reason for joining" in your request.**

If your question is about a specific model that you're working on, then the guidelines for asking a good question are similar to filing a good bug report, so again, please read the paper [<u>Ten simple rules on reporting a bug</u>](https://journals.plos.org/ploscompbiol/article?id=10.1371/journal.pcbi.1010540). For example: search slim-discuss for previous answers to your question; try to answer your question yourself using the manual first; whittle down your model to a minimal reproducible example; post the complete model code so that we can see what you're actually doing; clearly state what you're trying to do and why; if you're getting an error message, copy/paste that error message rather than just saying "I got an error"; and so forth. Be specific and give us the information we will need to answer your question; don't make us guess.

A particularly common type of question is along the lines of:

#### "Why doesn't my model's behavior match theory?"

These questions can be tricky to answer.  The reason they are tricky is because "theory" can mean a lot of different things, and for that matter, so can "match". So, this sort of question is much more likely to get a useful and interesting answer if it includes:

* an explicit description of what the theoretical result is

* a plot of the observed and expected values as they change with some parameter

* citation of somewhere that explains/derives the theoretical result

* ideally, an overview of what assumptions and approximations underly the result (e.g., "under the WF model for large N with infinite sites mutations")

* a high-level description of the SLiM model and how the relevant statistics are calculated
the SLiM recipe and other code, ideally set up to run without command-line arguments or input files on someone else's computer, made as simple as possible

These sorts of questions are pretty common, and natural, and it's not obvious to many people how a question like this can be hard to answer without additional information of this sort.

Often the results from simulation don't match theory because they're not *expected* to match theory; the simulation violates assumptions used by the theory in important ways, or the theory is known to be limited or biased or approximate, or the simulations – being stochastic – don't exactly match theory simply due to stochasticity. Try to think about these possibilities and rule them out before asking us. We don't want to discourage relevant questions, but keep in mind that we're here to answer questions about our software, not to provide general consulting services for any and all questions about population genetics. :->


## Opening a discussion

If you want to start a conversation with the SLiM community about an idea, perhaps the best place for it is the new [<u>slim-community organization</u>](https://github.com/slim-community) on GitHub.  Just click the "Request to Join" button to join, and once you receive your invite from a community moderator, you're good to go!  To give your discussion visibility, please feel free to post, *just once*, on slim-discuss to tell people that your discussion is there.

The slim-discuss mailing list might also be a good place to post, but please do be aware that hundreds of people subscribe to that mailing list; think about whether you really want to put something in all those people's inboxes, or whether you might want to just bounce the idea off of the developers – us – in a GitHub issue, or make a slim-community discussion that people can join in on. But if you think your idea is really appropriate for the mailing list, then go for it!


## SLiM-related job postings

If you are looking to hire someone with SLiM skills, or you have SLiM skills and you're looking to get hired, please post in the [<u>slim-community organization</u>](https://github.com/slim-community), specifically in the [<u>Jobs discussion area</u>](https://github.com/orgs/slim-community/discussions/categories/jobs).

The [<u>EvolDir mailing list</u>](https://evol.mcmaster.ca/evoldir.html) is also a good place to watch for job listings (or post your own), and does sometimes have listings that specifically mention SLiM.

Good luck!


## Contributing code

**First of all, all contributions to SLiM must be human-authored in accordance with the code of conduct.**  It is acceptable to use AI as a reference tool, to essentially provide documentation that you consult while working.  It is *not* acceptable to use AI to write code, documentation, or any other part of a PR that you submit to the SLiM repository.  PRs that incorporate AI-generated content will be rejected, with reference to this policy.  Repeated attempts will result in blocking.  Thank you for understanding and complying with this policy.

If you wish to make a contribution to SLiM, it would be a good idea to open an issue and get feedback first.  This repository is under intensive development, with specific goals in mind; contributions that don't fit with the overall vision of SLiM, or that conflict heavily with other changes underway or planned, may be rejected.  Discussing your planned changes before you make them would help you to craft a contribution that would be accepted.  That said, PRs that make a useful contribution, such as adding a new function into Eidos, are quite welcome!

The standard workflow for contributions is to fork and open a pull request (PR) based upon a branch in your fork.


## Contributing documentation and recipes.

**First of all, all contributions to SLiM must be human-authored in accordance with the code of conduct.**  That said, if you wish to make contributions to SLiM's documentation, such as edits or new recipes, that's great.  SLiM's documentation is kept in a macOS app called Pages that stores its data in binary form, so the doc is not available online in its original form, and pull requests against it are not possible.  In general the best approach is thus to open a new issue and provide a description of the edits you would suggest, with a rationale for them.

For new recipes/models, unless you believe that they are of sufficient general interest to belong in the SLiM manual itself, the best place to contribute is probably the [<u>SLiM-Extras repository</u>](https://github.com/MesserLab/SLiM-Extras).  New pull requests for SLiM-Extras are generally welcome.  Please edit the appropriate README files to add a mention of your contribution, and please provide a credit to yourself, including your name, institution, and an email address.


## Contributing whole repositories

If you have a contribution that is large enough to make sense as its own standalone repository, such as a new software tool that could join the SLiM ecosystem, the best home is the [<u>slim-community organization</u>](https://github.com/slim-community) on GitHub.  You can request to join that organization by clicking the big "Request to Join" button, and you can see some existing contributed repositories there.


## Closing remarks

Thanks for reading this and thinking about the best way to engage; we appreciate it.

And thanks for being a part of the SLiM community!  Happy SLiMulating!

*&mdash; Ben Haller, 30 December 2025*
