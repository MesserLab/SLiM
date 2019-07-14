#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"
#include "QtSLiMAppDelegate.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QtDebug>

#include <unistd.h>

#include "individual.h"


// TO DO:
//
// custom layout for play/profile buttons: https://doc.qt.io/qt-5/layout.html
// splitviews for the window: https://stackoverflow.com/questions/28309376/how-to-manage-qsplitter-in-qt-designer
// set up the app icon correctly: this seems to be very complicated, and didn't work on macOS, sigh...
// get the tableview columns configured correctly
// make the chromosome view
// make the population view
// syntax coloring in the script and output textedits
// implement pop-up menu for graph pop-up button
// implement continuous play

QtSLiMWindow::QtSLiMWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::QtSLiMWindow)
{
    // set default viewing state; this might come from user defaults on a per-script basis eventually...
    zoomedChromosomeShowsRateMaps = false;
    zoomedChromosomeShowsGenomicElements = false;
    zoomedChromosomeShowsMutations = true;
    zoomedChromosomeShowsFixedSubstitutions = false;

    // create the window UI
    ui->setupUi(this);
    initializeUI();

    // We set the working directory for new windows to ~/Desktop/, since it makes no sense for them to use the location of the app.
    // Each running simulation will track its own working directory, and the user can set it with a button in the SLiMgui window.
    sim_working_dir = Eidos_ResolvedPath("~/Desktop");
    sim_requested_working_dir = sim_working_dir;	// return to Desktop on recycle unless the user overrides it
}

QtSLiMWindow::~QtSLiMWindow()
{
    delete ui;

    if (sim)
    {
        delete sim;
        sim = nullptr;
    }
//    if (slimgui)
//	{
//		delete slimgui;
//		slimgui = nullptr;
//	}

    Eidos_FreeRNG(sim_RNG);

    setInvalidSimulation(true);
}

void QtSLiMWindow::initializeUI(void)
{
    glueUI();
    
    // fix the layout of the window
    ui->scriptHeaderLayout->setSpacing(4);
    ui->scriptHeaderLayout->setMargin(0);
    ui->scriptHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->outputHeaderLayout->setSpacing(4);
    ui->outputHeaderLayout->setMargin(0);
    ui->outputHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->chromosomeButtonsLayout->setSpacing(4);
    ui->chromosomeButtonsLayout->setMargin(0);

    ui->playControlsLayout->setSpacing(4);
    ui->playControlsLayout->setMargin(0);

    // set up the script and output textedits
    int tabWidth = 0;
    QFont &scriptFont = QtSLiMWindow::defaultScriptFont(&tabWidth);

    ui->scriptTextEdit->setFont(scriptFont);
    ui->scriptTextEdit->setTabStopWidth(tabWidth);    // should use setTabStopDistance(), which requires Qt 5.10; see https://stackoverflow.com/a/54605709/2752221

    ui->outputTextEdit->setFont(scriptFont);
    ui->scriptTextEdit->setTabStopWidth(tabWidth);    // should use setTabStopDistance(), which requires Qt 5.10; see https://stackoverflow.com/a/54605709/2752221

    // remove the profile button, for the time being
    QPushButton *profileButton = ui->profileButton;

    ui->playControlsLayout->removeWidget(profileButton);
    delete profileButton;

    // set button states
    ui->showChromosomeMapsButton->setChecked(zoomedChromosomeShowsRateMaps);
    ui->showGenomicElementsButton->setChecked(zoomedChromosomeShowsGenomicElements);
    ui->showMutationsButton->setChecked(zoomedChromosomeShowsMutations);
    ui->showFixedSubstitutionsButton->setChecked(zoomedChromosomeShowsFixedSubstitutions);

    // set up the initial script
    scriptString = QtSLiMWindow::defaultWFScriptString();
    ui->scriptTextEdit->setText(QString::fromStdString(scriptString));
    setScriptStringAndInitializeSimulation(scriptString);
    
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &QtSLiMWindow::scriptTexteditChanged);

    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
}

QFont &QtSLiMWindow::defaultScriptFont(int *p_tabWidth)
{
    static QFont *scriptFont = nullptr;
    static int tabWidth = 0;
    
    if (!scriptFont)
    {
        QFontDatabase fontdb;
        QStringList families = fontdb.families();
        
        //qDebug() << families;
        
        // Use filter() to look for matches, since the foundry can be appended after the name (why isn't this easier??)
        // FIXME should have preferences UI for choosing the font and size
        if (families.filter("DejaVu Sans Mono").size() > 0)
            scriptFont = new QFont("DejaVu Sans Mono", 9);
        else if (families.filter("Source Code Pro").size() > 0)
            scriptFont = new QFont("Source Code Pro", 9);
        else if (families.filter("Menlo").size() > 0)
            scriptFont = new QFont("Menlo", 11);
        else
            scriptFont = new QFont("Courier", 9);
        
        //qDebug() << "Chosen font: " << scriptFont->family();
        
        QFontMetrics fm(*scriptFont);
        
        //tabWidth = fm.horizontalAdvance("   ");   // added in Qt 5.11
        tabWidth = fm.width("   ");                 // deprecated (in 5.11, I assume)
    }
    
    *p_tabWidth = tabWidth;
    return *scriptFont;
}

std::string QtSLiMWindow::defaultWFScriptString(void)
{
    return std::string(
                "// set up a simple neutral simulation\n"
                "initialize() {\n"
                "	initializeMutationRate(1e-7);\n"
                "	\n"
                "	// m1 mutation type: neutral\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	\n"
                "	// g1 genomic element type: uses m1 for all mutations\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	\n"
                "	// uniform chromosome of length 100 kb with uniform recombination\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// create a population of 500 individuals\n"
                "1 {\n"
                "	sim.addSubpop(\"p1\", 500);\n"
                "}\n"
                "\n"
                "// output samples of 10 genomes periodically, all fixed mutations at end\n"
                "1000 late() { p1.outputSample(10); }\n"
                "2000 late() { p1.outputSample(10); }\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

std::string QtSLiMWindow::defaultNonWFScriptString(void)
{
    return std::string(
                "// set up a simple neutral nonWF simulation\n"
                "initialize() {\n"
                "	initializeSLiMModelType(\"nonWF\");\n"
                "	defineConstant(\"K\", 500);	// carrying capacity\n"
                "	\n"
                "	// neutral mutations, which are allowed to fix\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	m1.convertToSubstitution = T;\n"
                "	\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeMutationRate(1e-7);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// each individual reproduces itself once\n"
                "reproduction() {\n"
                "	subpop.addCrossed(individual, p1.sampleIndividuals(1));\n"
                "}\n"
                "\n"
                "// create an initial population of 10 individuals\n"
                "1 early() {\n"
                "	sim.addSubpop(\"p1\", 10);\n"
                "}\n"
                "\n"
                "// provide density-dependent selection\n"
                "early() {\n"
                "	p1.fitnessScaling = K / p1.individualCount;\n"
                "}\n"
                "\n"
                "// output all fixed mutations at end\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

void QtSLiMWindow::setInvalidSimulation(bool p_invalid)
{
    invalidSimulation_ = p_invalid;
}

void QtSLiMWindow::setReachedSimulationEnd(bool p_reachedEnd)
{
    reachedSimulationEnd_ = p_reachedEnd;
}

void QtSLiMWindow::checkForSimulationTermination(void)
{
    std::string &&terminationMessage = gEidosTermination.str();

    if (!terminationMessage.empty())
    {
        const char *cstr = terminationMessage.c_str();
        //std::string str(cstr);

        gEidosTermination.clear();
        gEidosTermination.str("");

        qDebug() << cstr;
        //[self performSelector:@selector(showTerminationMessage:) withObject:[str retain] afterDelay:0.0];	// showTerminationMessage: will release the string

        // Now we need to clean up so we are in a displayable state.  Note that we don't even attempt to dispose
        // of the old simulation object; who knows what state it is in, touching it might crash.
        sim = nullptr;
        //slimgui = nullptr;

        Eidos_FreeRNG(sim_RNG);

        setReachedSimulationEnd(true);
        setInvalidSimulation(true);
    }
}

void QtSLiMWindow::startNewSimulationFromScript(void)
{
    if (sim)
    {
        delete sim;
        sim = nullptr;
    }
//    if (slimgui)
//    {
//        delete slimgui;
//        slimgui = nullptr;
//    }

    // Free the old simulation RNG and let SLiM make one for us
    Eidos_FreeRNG(sim_RNG);

    if (EIDOS_GSL_RNG)
        qDebug() << "gEidos_RNG already set up in startNewSimulationFromScript!";

    std::istringstream infile(scriptString);

    try
    {
        sim = new SLiMSim(infile);
        sim->InitializeRNGFromSeed(nullptr);

        // We take over the RNG instance that SLiMSim just made, since each SLiMgui window has its own RNG
        sim_RNG = gEidos_RNG;
        EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));

        // We also reset various Eidos/SLiM instance state; each SLiMgui window is independent
        sim_next_pedigree_id = 0;
        sim_next_mutation_id = 0;
        sim_suppress_warnings = false;

        // The current working directory was set up in -init to be ~/Desktop, and should not be reset here; if the
        // user has changed it, that change ought to stick across recycles.  So this bounces us back to the last dir chosen.
        sim_working_dir = sim_requested_working_dir;

        setReachedSimulationEnd(false);
        setInvalidSimulation(false);
        hasImported_ = false;
    }
    catch (...)
    {
        if (sim)
            sim->simulation_valid_ = false;
        setReachedSimulationEnd(true);
        checkForSimulationTermination();
    }

    if (sim)
    {
        // make a new SLiMgui instance to represent SLiMgui in Eidos
        //slimgui = new SLiMgui(*sim, self);

        // set up the "slimgui" symbol for it immediately
        //sim->simulation_constants_->InitializeConstantSymbolEntry(slimgui->SymbolTableEntry());
    }
}

void QtSLiMWindow::setScriptStringAndInitializeSimulation(std::string string)
{
    scriptString = string;
    startNewSimulationFromScript();
}

void QtSLiMWindow::updateOutputTextView(void)
{
    std::string &&newOutput = gSLiMOut.str();
	
	if (!newOutput.empty())
	{
        QString str = QString::fromStdString(newOutput);
		
		// So, ideally we would stay pinned at the bottom if the user had scrolled to the bottom, but would stay
		// at the user's chosen scroll position above the bottom if they chose such a position.  Unfortunately,
		// this doesn't seem to work.  I'm not quite sure why.  Particularly when large amounts of output get
		// added quickly, the scroller doesn't seem to catch up, and then it reads here as not being at the
		// bottom, and so we become unpinned even though we used to be pinned.  I'm going to just give up, for
		// now, and always scroll to the bottom when new output comes out.  That's what many other such apps
		// do anyway; it's a little annoying if you're trying to read old output, but so it goes.
		
		//NSScrollView *enclosingScrollView = [outputTextView enclosingScrollView];
		//BOOL scrolledToBottom = YES; //(![enclosingScrollView hasVerticalScroller] || [[enclosingScrollView verticalScroller] doubleValue] == 1.0);
		
        // ui->outputTextEdit->append(str) would seem the obvious thing to do, but that adds an extra newline (!),
        // so it can't be used.  WTF.  The solution here does not preserve the user's scroll position; see discussion at
        // https://stackoverflow.com/questions/13559990/how-to-append-text-to-qplaintextedit-without-adding-newline-and-keep-scroll-at
        // which has a complex solution involving subclassing QTextEdit... sigh...
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        ui->outputTextEdit->insertPlainText(str);
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        
		//if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightOutputKey])
		//	[outputTextView recolorAfterChanges];
		
		// if the user was scrolled to the bottom, we keep them there; otherwise, we let them stay where they were
		//if (scrolledToBottom)
		//	[outputTextView scrollRangeToVisible:NSMakeRange([[outputTextView string] length], 0)];
		
		// clear any error flags set on the stream and empty out its string so it is ready to receive new output
		gSLiMOut.clear();
		gSLiMOut.str("");
	}
}

void QtSLiMWindow::updateGenerationCounter(void)
{
    if (!invalidSimulation())
	{
		if (sim->generation_ == 0)
            ui->generationLineEdit->setText("initialize()");
		else
            ui->generationLineEdit->setText(QString::number(sim->generation_));
	}
	else
        ui->generationLineEdit->setText("");
}

void QtSLiMWindow::updateAfterTickFull(bool fullUpdate)
{
    // fullUpdate is used to suppress some expensive updating to every third update
	if (!fullUpdate)
	{
		if (++partialUpdateCount_ >= 3)
		{
			partialUpdateCount_ = 0;
			fullUpdate = true;
		}
	}
	
	// Check whether the simulation has terminated due to an error; if so, show an error message with a delayed perform
	checkForSimulationTermination();
	
	// The rest of the code here needs to be careful about the invalid state; we do want to update our controls when invalid, but sim is nil.
	//bool invalid = invalidSimulation();
	
	if (fullUpdate)
	{
		// FIXME it would be good for this updating to be minimal; reloading the tableview every time, etc., is quite wasteful...
		updateOutputTextView();
		
		// Reloading the subpop tableview is tricky, because we need to preserve the selection across the reload, while also noting that the selection is forced
		// to change when a subpop goes extinct.  The current selection is noted in the gui_selected_ ivar of each subpop.  So what we do here is reload the tableview
		// while suppressing our usual update of our selection state, and then we try to re-impose our selection state on the new tableview content.  If a subpop
		// went extinct, we will fail to notice the selection change; but that is OK, since we force an update of populationView and chromosomeZoomed below anyway.
//		reloadingSubpopTableview = true;
//		[subpopTableView reloadData];
		
//		if (invalid || !sim)
//		{
//			[subpopTableView deselectAll:nil];
//		}
//		else
//		{
//			Population &population = sim->population_;
//			int subpopCount = (int)population.subpops_.size();
//			auto popIter = population.subpops_.begin();
//			NSMutableIndexSet *indicesToSelect = [NSMutableIndexSet indexSet];
			
//			for (int i = 0; i < subpopCount; ++i)
//			{
//				if (popIter->second->gui_selected_)
//					[indicesToSelect addIndex:i];
				
//				popIter++;
//			}
			
//			[subpopTableView selectRowIndexes:indicesToSelect byExtendingSelection:NO];
//		}
		
//		reloadingSubpopTableview = false;
//		[subpopTableView setNeedsDisplay];
	}
	
	// Now update our other UI, some of which depends upon the state of subpopTableView 
//	[populationView setNeedsDisplay:YES];
//	[chromosomeZoomed setNeedsDisplayInInterior];
	
//	[self updatePopulationViewHiding];
	
	if (fullUpdate)
		updateGenerationCounter();
	
	// Update stuff that only needs updating when the script is re-parsed, not after every tick
//	if (invalid || sim->mutation_types_changed_)
//	{
//		[mutTypeTableView reloadData];
//		[mutTypeTableView setNeedsDisplay];
		
//		if (sim)
//			sim->mutation_types_changed_ = false;
//	}
	
//	if (invalid || sim->genomic_element_types_changed_)
//	{
//		[genomicElementTypeTableView reloadData];
//		[genomicElementTypeTableView setNeedsDisplay];
		
//		if (sim)
//			sim->genomic_element_types_changed_ = false;
//	}
	
//	if (invalid || sim->interaction_types_changed_)
//	{
//		[interactionTypeTableView reloadData];
//		[interactionTypeTableView setNeedsDisplay];
		
//		if (sim)
//			sim->interaction_types_changed_ = false;
//	}
	
//	if (invalid || sim->scripts_changed_)
//	{
//		[scriptBlocksTableView reloadData];
//		[scriptBlocksTableView setNeedsDisplay];
		
//		if (sim)
//			sim->scripts_changed_ = false;
//	}
	
//	if (invalid || sim->chromosome_changed_)
//	{
//		[chromosomeOverview restoreLastSelection];
//		[chromosomeOverview setNeedsDisplay:YES];
		
//		if (sim)
//			sim->chromosome_changed_ = false;
//	}
	
	// Update graph windows as well; this will usually trigger a setNeedsDisplay:YES but may do other updating work as well
//	if (fullUpdate)
//		[self sendAllLinkedViewsSelector:@selector(updateAfterTick)];
}

void QtSLiMWindow::updateRecycleButtonIcon(bool pressed)
{
    if (slimChangeCount)
        ui->recycleButton->setIcon(QIcon(pressed ? ":/buttons/recycle_GH.png" : ":/buttons/recycle_G.png"));
    else
        ui->recycleButton->setIcon(QIcon(pressed ? ":/buttons/recycle_H.png" : ":/buttons/recycle.png"));
}


//
//  simulation play mechanics
//

void QtSLiMWindow::willExecuteScript(void)
{
    // Whenever we are about to execute script, we swap in our random number generator; at other times, gEidos_rng is NULL.
    // The goal here is to keep each SLiM window independent in its random number sequence.
    if (EIDOS_GSL_RNG)
        qDebug() << "eidosConsoleWindowControllerWillExecuteScript: gEidos_rng already set up!";

    gEidos_RNG = sim_RNG;

    // We also swap in the pedigree id and mutation id counters; each SLiMgui window is independent
    gSLiM_next_pedigree_id = sim_next_pedigree_id;
    gSLiM_next_mutation_id = sim_next_mutation_id;
    gEidosSuppressWarnings = sim_suppress_warnings;

    // Set the current directory to its value for this window
    errno = 0;
    int retval = chdir(sim_working_dir.c_str());

    if (retval == -1)
        qDebug() << "willExecuteScript: Unable to set the working directory to " << sim_working_dir.c_str() << " (error " << errno << ")";
}

void QtSLiMWindow::didExecuteScript(void)
{
    // Swap our random number generator back out again; see -eidosConsoleWindowControllerWillExecuteScript
    sim_RNG = gEidos_RNG;
    EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));

    // Swap out our pedigree id and mutation id counters; see -eidosConsoleWindowControllerWillExecuteScript
    // Setting to -100000 here is not necessary, but will maybe help find bugs...
    sim_next_pedigree_id = gSLiM_next_pedigree_id;
    gSLiM_next_pedigree_id = -100000;

    sim_next_mutation_id = gSLiM_next_mutation_id;
    gSLiM_next_mutation_id = -100000;

    sim_suppress_warnings = gEidosSuppressWarnings;
    gEidosSuppressWarnings = false;

    // Get the current working directory; each SLiM window has its own cwd, which may have been changed in script since ...WillExecuteScript:
    sim_working_dir = Eidos_CurrentDirectory();

    // Return to the app's working directory when not running SLiM/Eidos code
    std::string &app_cwd = qtSLiMAppDelegate->QtSLiMCurrentWorkingDirectory();
    errno = 0;
    int retval = chdir(app_cwd.c_str());

    if (retval == -1)
        qDebug() << "didExecuteScript: Unable to set the working directory to " << app_cwd.c_str() << " (error " << errno << ")";
}

bool QtSLiMWindow::runSimOneGeneration(void)
{
    // This method should always be used when calling out to run the simulation, because it swaps the correct random number
    // generator stuff in and out bracketing the call to RunOneGeneration().  This bracketing would need to be done around
    // any other call out to the simulation that caused it to use random numbers, too, such as subsample output.
    bool stillRunning = true;

    willExecuteScript();

//	if (profilePlayOn)
//	{
//		// We put the wall clock measurements on the inside since we want those to be maximally accurate,
//		// as profile report percentages are fractions of the total elapsed wall clock time.
//		clock_t startCPUClock = clock();
//		SLIM_PROFILE_BLOCK_START();

//		stillRunning = sim->RunOneGeneration();

//		SLIM_PROFILE_BLOCK_END(profileElapsedWallClock);
//		clock_t endCPUClock = clock();

//		profileElapsedCPUClock += (endCPUClock - startCPUClock);
//	}
//	else
    {
        stillRunning = sim->RunOneGeneration();
    }

    didExecuteScript();

    // We also want to let graphViews know when each generation has finished, in case they need to pull data from the sim.  Note this
    // happens after every generation, not just when we are updating the UI, so drawing and setNeedsDisplay: should not happen here.
    //[self sendAllLinkedViewsSelector:@selector(controllerGenerationFinished)];

    return stillRunning;
}


//
//  change tracking and the recycle button
//

// Do our own tracking of the change count.  We do this so that we know whether the script is in
// the same state it was in when we last recycled, or has been changed.  If it has been changed,
// we add a highlight under the recycle button to suggest to the user that they might want to
// recycle to bring their changes into force.
void QtSLiMWindow::updateChangeCount(void) //:(NSDocumentChangeType)change
{
	//[super updateChangeCount:change];
	
	// Mask off flags in the high bits.  Apple is not explicit about this, but NSChangeDiscardable
	// is 256, and acts as a flag bit, so it seems reasonable to assume this for future compatibility.
//	NSDocumentChangeType maskedChange = (NSDocumentChangeType)(change & 0x00FF);
	
//	if ((maskedChange == NSChangeDone) || (maskedChange == NSChangeRedone))
		slimChangeCount++;
//	else if (maskedChange == NSChangeUndone)
//		slimChangeCount--;
	
	updateRecycleButtonIcon(false);
}

bool QtSLiMWindow::changedSinceRecycle(void)
{
	return !(slimChangeCount == 0);
}

void QtSLiMWindow::resetSLiMChangeCount(void)
{
    slimChangeCount = 0;
	
	updateRecycleButtonIcon(false);
}

// slot receiving the signal QTextEdit::textChanged() from the script textedit
void QtSLiMWindow::scriptTexteditChanged(void)
{
    updateChangeCount();
}


//
//  public slots
//

void QtSLiMWindow::playOneStepClicked(void)
{
    if (!invalidSimulation())
    {
        //[_consoleController invalidateSymbolTableAndFunctionMap];
        setReachedSimulationEnd(!runSimOneGeneration());
        //[_consoleController validateSymbolTableAndFunctionMap];
        updateAfterTickFull(true);
    }
}

void QtSLiMWindow::playClicked(void)
{
    ui->playButton->setIcon(QIcon(ui->playButton->isChecked() ? ":/buttons/play_H.png" : ":/buttons/play.png"));

    qDebug() << "playClicked: isChecked() == " << ui->playButton->isChecked();
}

void QtSLiMWindow::profileClicked(void)
{
    ui->profileButton->setIcon(QIcon(ui->profileButton->isChecked() ? ":/buttons/profile_H.png" : ":/buttons/profile.png"));

    qDebug() << "profileClicked: isChecked() == " << ui->profileButton->isChecked();
}

void QtSLiMWindow::recycleClicked(void)
{
    // Converting a QString to a std::string is surprisingly tricky: https://stackoverflow.com/a/4644922/2752221
    std::string utf8_script_string = ui->scriptTextEdit->toPlainText().toUtf8().constData();
    
    //[_consoleController invalidateSymbolTableAndFunctionMap];
	clearOutputClicked();
    setScriptStringAndInitializeSimulation(utf8_script_string);
	//[_consoleController validateSymbolTableAndFunctionMap];
	updateAfterTickFull(true);
	
	// A bit of playing with undo.  We want to break undo coalescing at the point of recycling, so that undo and redo stop
	// at the moment that we recycled.  Then we reset a change counter that we use to know if we have changed relative to
	// the recycle point, so we can highlight the recycle button to show that the executing script is out of date.
	//[scriptTextView breakUndoCoalescing];
	resetSLiMChangeCount();

	//[self sendAllLinkedViewsSelector:@selector(controllerRecycled)];
}

void QtSLiMWindow::showMutationsToggled(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));

    qDebug() << "showMutationsToggled: isChecked() == " << ui->showMutationsButton->isChecked();
}

void QtSLiMWindow::showFixedSubstitutionsToggled(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));

    qDebug() << "showFixedSubstitutionsToggled: isChecked() == " << ui->showFixedSubstitutionsButton->isChecked();
}

void QtSLiMWindow::showChromosomeMapsToggled(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));

    qDebug() << "showRecombinationIntervalsToggled: isChecked() == " << ui->showChromosomeMapsButton->isChecked();
}

void QtSLiMWindow::showGenomicElementsToggled(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));

    qDebug() << "showGenomicElementsToggled: isChecked() == " << ui->showGenomicElementsButton->isChecked();
}

void QtSLiMWindow::checkScriptClicked(void)
{
    qDebug() << "checkScriptClicked";
}

void QtSLiMWindow::prettyprintClicked(void)
{
    qDebug() << "prettyprintClicked";
}

void QtSLiMWindow::scriptHelpClicked(void)
{
    qDebug() << "showHelpClicked";
}

void QtSLiMWindow::showConsoleClicked(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));

    qDebug() << "showConsoleClicked: isChecked() == " << ui->consoleButton->isChecked();
}

void QtSLiMWindow::showBrowserClicked(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));

    qDebug() << "showBrowserClicked: isChecked() == " << ui->browserButton->isChecked();
}

void QtSLiMWindow::clearOutputClicked(void)
{
    ui->outputTextEdit->setText("");
}

void QtSLiMWindow::dumpPopulationClicked(void)
{
    qDebug() << "dumpPopulationClicked";
}

void QtSLiMWindow::graphPopupButtonClicked(void)
{
    qDebug() << "graphButtonClicked";
}

void QtSLiMWindow::changeDirectoryClicked(void)
{
    qDebug() << "changeDirectoryClicked";
}





















