//
//  QtSLiMChromosomeWidget.cpp
//  SLiM
//
//  Created by Ben Haller on 7/28/2019.
//  Copyright (c) 2019-2024 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "QtSLiMChromosomeWidget.h"
#include "QtSLiMWindow.h"
#include "QtSLiMExtras.h"
#include "QtSLiMHaplotypeManager.h"
#include "QtSLiMPreferences.h"

#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMouseEvent>
#include <QtDebug>

#include <map>
#include <algorithm>
#include <vector>


static const int numberOfTicksPlusOne = 4;
static const int tickLength = 5;
static const int heightForTicks = 16;
static const int selectionKnobSizeExtension = 2;	// a 5-pixel-width knob is 2: 2 + 1 + 2, an extension on each side plus the one pixel of the bar in the middle
static const int selectionKnobSize = selectionKnobSizeExtension + selectionKnobSizeExtension + 1;
static const int spaceBetweenChromosomes = 5;


QtSLiMChromosomeWidgetController::QtSLiMChromosomeWidgetController(QtSLiMWindow *slimWindow, QWidget *displayWindow, Species *focalSpecies) :
    QObject(displayWindow ? displayWindow : slimWindow),
    slimWindow_(slimWindow),
    displayWindow_(displayWindow)
{
    connect(slimWindow_, &QtSLiMWindow::controllerPartialUpdateAfterTick, this, &QtSLiMChromosomeWidgetController::updateFromController);
    
    // focalSpecies is used only in the displayWindow case, for showing the species badge in multispecies models
    // otherwise, slimWindow will control the focal display species for our chromosome view as it requires
    if (displayWindow)
    {
        if (!focalSpecies)
        {
            qDebug() << "no focal species for creating a chromosome display!";
            return;
        }
        
        focalSpeciesName_ = focalSpecies->name_;
        // focalSpeciesAvatar_ is set up in buildChromosomeDisplay();
    }
}

void QtSLiMChromosomeWidgetController::updateFromController(void)
{
    if (displayWindow_)
    {
        Species *displaySpecies = focalDisplaySpecies();
        
        if (displaySpecies)
        {
            Community *community = slimWindow_->community;
            
            if (needsRebuild_ && !invalidSimulation() && community->simulation_valid_ && (community->tick_ >= 1))
            {
                // It's hard to tell, in general, whether we need a rebuild: if the number of
                // chromosomes has changed, or the length of any chromosome, or the symbol of
                // any chromosome, etc.  There's no harm, so we just always rebuild at the
                // first valid moment after recycling.
                buildChromosomeDisplay(false);
                needsRebuild_ = false;
            }
        }
        else
        {
            // we've just recycled or become invalid; our next update should rebuild the display
            needsRebuild_ = true;
        }
    }
    
    emit needsRedisplay();
}

Species *QtSLiMChromosomeWidgetController::focalDisplaySpecies(void)
{
    if (displayWindow_)
    {
        // with a chromosome display, we are not based on the current focal species of slimWindow_, so we
        // need to look up the focal display species dynamically based on its name (which could fail)
        if (focalSpeciesName_.length() == 0)
            return nullptr;
        
        if (slimWindow_ && slimWindow_->community && (slimWindow_->community->Tick() >= 1))
            return slimWindow_->community->SpeciesWithName(focalSpeciesName_);
        
        return nullptr;
    }
    
    // otherwise, our focal display species comes directly from slimWindow_
    return slimWindow_->focalDisplaySpecies();
}

void QtSLiMChromosomeWidgetController::buildChromosomeDisplay(bool resetWindowSize)
{
    // Remove any existing content from our display window and build new content
    if (!displayWindow_)
        return;
    
    // Assess the chromosomes to be displayed
    Species *focalSpecies = focalDisplaySpecies();
    const std::vector<Chromosome *> &chromosomes = focalSpecies->Chromosomes();
    int chromosomeCount = (int)chromosomes.size();
    slim_position_t chromosomeMaxLength = 0;
    
    for (Chromosome *chromosome : chromosomes)
    {
        slim_position_t length = chromosome->last_position_ + 1;
        
        chromosomeMaxLength = std::max(chromosomeMaxLength, length);
    }
    
    // Deal with window sizing
    const int margin = 5;
    const int spacing = 5;
    const int buttonRowHeight = margin + margin + 20;
    
    displayWindow_->setMinimumSize(500, margin + 20 * chromosomeCount + spacing * (chromosomeCount - 1) + buttonRowHeight);
    displayWindow_->setMaximumSize(4096, margin + 200 * chromosomeCount + spacing * (chromosomeCount - 1) + buttonRowHeight);
    if (resetWindowSize)
        displayWindow_->resize(800, margin + 30 * chromosomeCount + spacing * (chromosomeCount - 1) + buttonRowHeight);
    
    // Find the top-level layout and remove all of its current children
    QVBoxLayout *topLayout = qobject_cast<QVBoxLayout *>(displayWindow_->layout());
    
    QtSLiMClearLayout(topLayout, /* deleteWidgets */ true);
    
    // Add a chromosome view for each chromosome in the model, with a spacer next to it to give it the right length
    std::vector<QLabel *> labels;
    bool firstRow = true;
    
    for (Chromosome *chromosome : chromosomes)
    {
        QHBoxLayout *rowLayout = new QHBoxLayout;
        
        rowLayout->setContentsMargins(margin, firstRow ? margin : spacing, margin, 0);
        rowLayout->setSpacing(0);
        topLayout->addLayout(rowLayout);
        
        QtSLiMChromosomeWidget *chromosomeWidget = new QtSLiMChromosomeWidget(nullptr);
        
        chromosomeWidget->setController(this);
        chromosomeWidget->setFocalChromosome(chromosome);
        chromosomeWidget->setDisplayedRange(QtSLiMRange(0, 0)); // display entirety
        chromosomeWidget->setShowsTicks(false);
        
        slim_position_t length = chromosome->last_position_ + 1;
        double fractionOfMax = length / (double)chromosomeMaxLength;
        int chromosomeStretch = (int)(round(fractionOfMax * 255));  // Qt requires a max value of 255
        
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(useScaledWidths_ ? chromosomeStretch : 0);
        sizePolicy1.setVerticalStretch(0);
        chromosomeWidget->setSizePolicy(sizePolicy1);
        
        QLabel *chromosomeLabel = new QLabel();
        chromosomeLabel->setText(QString::fromStdString(chromosome->symbol_));
        chromosomeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Expanding);
        chromosomeLabel->setSizePolicy(sizePolicy2);
        
        rowLayout->addWidget(chromosomeLabel);
        rowLayout->addSpacing(margin);
        rowLayout->addWidget(chromosomeWidget);
        
        if (useScaledWidths_)
            rowLayout->addStretch(255 - chromosomeStretch);     // the remaining width after chromosomeStretch
        
        labels.push_back(chromosomeLabel);
        
        firstRow = false;
    }
    
    // adjust all the labels to have the same width
    int maxWidth = 0;
    
    for (QLabel *label : labels)
        maxWidth = std::max(maxWidth, label->sizeHint().width());
    
    for (QLabel *label : labels)
        label->setMinimumWidth(maxWidth);
    
    // Add a horizontal layout at the bottom, for the action button
    QHBoxLayout *buttonLayout = nullptr;
    
    {
        buttonLayout = new QHBoxLayout;
        
        buttonLayout->setContentsMargins(margin, margin, margin, margin);
        buttonLayout->setSpacing(5);
        topLayout->addLayout(buttonLayout);
        
        // set up the species badge; note that unlike QtSLiMGraphView, we set it up here immediately,
        // since we are guaranteed to already have a valid species object, and then we don't update it
        focalSpeciesAvatar_ = focalSpecies->avatar_;
        
        if (focalSpeciesAvatar_.length() && (focalSpecies->community_.all_species_.size() > 1))
        {
            QLabel *speciesLabel = new QLabel();
            speciesLabel->setText(QString::fromStdString(focalSpeciesAvatar_));
            buttonLayout->addWidget(speciesLabel);
        }
        
        QSpacerItem *rightSpacer = new QSpacerItem(16, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
        buttonLayout->addItem(rightSpacer);
        
        // this code is based on the creation of executeScriptButton in ui_QtSLiMEidosConsole.h
        QtSLiMPushButton *actionButton = new QtSLiMPushButton(displayWindow_);
        actionButton->setObjectName(QString::fromUtf8("actionButton"));
        actionButton->setMinimumSize(QSize(20, 20));
        actionButton->setMaximumSize(QSize(20, 20));
        actionButton->setFocusPolicy(Qt::NoFocus);
        QIcon icon4;
        icon4.addFile(QtSLiMImagePath("action", false), QSize(), QIcon::Normal, QIcon::Off);
        icon4.addFile(QtSLiMImagePath("action", true), QSize(), QIcon::Normal, QIcon::On);
        actionButton->setIcon(icon4);
        actionButton->setIconSize(QSize(20, 20));
        actionButton->qtslimSetBaseName("action");
        actionButton->setCheckable(true);
        actionButton->setFlat(true);
#if QT_CONFIG(tooltip)
        actionButton->setToolTip("<html><head/><body><p>configure chromosome display</p></body></html>");
#endif // QT_CONFIG(tooltip)
        buttonLayout->addWidget(actionButton);
        
        connect(actionButton, &QPushButton::pressed, this, [actionButton, this]() { actionButton->qtslimSetHighlight(true); actionButtonRunMenu(actionButton); });
        connect(actionButton, &QPushButton::released, this, [actionButton]() { actionButton->qtslimSetHighlight(false); });
        
        // note that this action button has no enable/disable code anywhere, since it is happy to respond at all times
    }
}

void QtSLiMChromosomeWidgetController::runChromosomeContextMenuAtPoint(QPoint p_globalPoint)
{
    if (!slimWindow_)
        return;
    
    Community *community = slimWindow_->community;
    
    if (!invalidSimulation() && community && community->simulation_valid_)
    {
        QMenu contextMenu("chromosome_menu", slimWindow_);  // slimWindow_ is the parent in the sense that the menu is freed if slimWindow_ is freed
        QAction *scaledWidths = nullptr;
        QAction *unscaledWidths = nullptr;
        Species *focalSpecies = focalDisplaySpecies();
        
        if (displayWindow_ && focalSpecies && (focalSpecies->Chromosomes().size() > 1))
        {
            // only in multichromosome models, offer to scale the widths of the displayed chromosomes
            // according to their length or not, as the user prefers
            scaledWidths = contextMenu.addAction("Use Scaled Widths");
            scaledWidths->setCheckable(true);
            scaledWidths->setChecked(useScaledWidths_);
            
            unscaledWidths = contextMenu.addAction("Use Full Widths");
            unscaledWidths->setCheckable(true);
            unscaledWidths->setChecked(!useScaledWidths_);
            
            contextMenu.addSeparator();
        }
        
        QAction *displayMutations = contextMenu.addAction("Display Mutations");
        displayMutations->setCheckable(true);
        displayMutations->setChecked(shouldDrawMutations_);
        
        QAction *displaySubstitutions = contextMenu.addAction("Display Substitutions");
        displaySubstitutions->setCheckable(true);
        displaySubstitutions->setChecked(shouldDrawFixedSubstitutions_);
        
        QAction *displayGenomicElements = contextMenu.addAction("Display Genomic Elements");
        displayGenomicElements->setCheckable(true);
        displayGenomicElements->setChecked(shouldDrawGenomicElements_);
        
        QAction *displayRateMaps = contextMenu.addAction("Display Rate Maps");
        displayRateMaps->setCheckable(true);
        displayRateMaps->setChecked(shouldDrawRateMaps_);
        
        contextMenu.addSeparator();
        
        QAction *displayFrequencies = contextMenu.addAction("Display Frequencies");
        displayFrequencies->setCheckable(true);
        displayFrequencies->setChecked(!displayHaplotypes_);
        
        QAction *displayHaplotypes = contextMenu.addAction("Display Haplotypes");
        displayHaplotypes->setCheckable(true);
        displayHaplotypes->setChecked(displayHaplotypes_);
        
        QActionGroup *displayGroup = new QActionGroup(this);    // On Linux this provides a radio-button-group appearance
        displayGroup->addAction(displayFrequencies);
        displayGroup->addAction(displayHaplotypes);
        
        QAction *displayAllMutations = nullptr;
        QAction *selectNonneutralMutations = nullptr;
        
        // mutation type checkmark items
        {
            const std::map<slim_objectid_t,MutationType*> &muttypes = community->AllMutationTypes();
            
            if (muttypes.size() > 0)
            {
                contextMenu.addSeparator();
                
                displayAllMutations = contextMenu.addAction("Display All Mutations");
                displayAllMutations->setCheckable(true);
                displayAllMutations->setChecked(displayMuttypes_.size() == 0);
                
                // Make a sorted list of all mutation types we know – those that exist, and those that used to exist that we are displaying
                std::vector<slim_objectid_t> all_muttypes;
                
                for (auto muttype_iter : muttypes)
                {
                    MutationType *muttype = muttype_iter.second;
                    slim_objectid_t muttype_id = muttype->mutation_type_id_;
                    
                    all_muttypes.emplace_back(muttype_id);
                }
                
                all_muttypes.insert(all_muttypes.end(), displayMuttypes_.begin(), displayMuttypes_.end());
                
                // Avoid building a huge menu, which will hang the app
                if (all_muttypes.size() <= 500)
                {
                    std::sort(all_muttypes.begin(), all_muttypes.end());
                    all_muttypes.resize(static_cast<size_t>(std::distance(all_muttypes.begin(), std::unique(all_muttypes.begin(), all_muttypes.end()))));
                    
                    // Then add menu items for each of those muttypes
                    for (slim_objectid_t muttype_id : all_muttypes)
                    {
                        QString menuItemTitle = QString("Display m%1").arg(muttype_id);
                        MutationType *muttype = community->MutationTypeWithID(muttype_id);  // try to look up the mutation type; can fail if it doesn't exists now
                        
                        if (muttype && (community->all_species_.size() > 1))
                            menuItemTitle.append(" ").append(QString::fromStdString(muttype->species_.avatar_));
                        
                        QAction *mutationAction = contextMenu.addAction(menuItemTitle);
                        
                        mutationAction->setData(muttype_id);
                        mutationAction->setCheckable(true);
                        
                        if (std::find(displayMuttypes_.begin(), displayMuttypes_.end(), muttype_id) != displayMuttypes_.end())
                            mutationAction->setChecked(true);
                    }
                }
                
                contextMenu.addSeparator();
                
                selectNonneutralMutations = contextMenu.addAction("Select Non-Neutral MutationTypes");
            }
        }
        
        // Run the context menu synchronously
        QAction *action = contextMenu.exec(p_globalPoint);
        
        // Act upon the chosen action; we just do it right here instead of dealing with slots
        if (action)
        {
            if (action == scaledWidths)
            {
                if (!useScaledWidths_)
                {
                    useScaledWidths_ = true;
                    buildChromosomeDisplay(false);
                }
            }
            else if (action == unscaledWidths)
            {
                if (useScaledWidths_)
                {
                    useScaledWidths_ = false;
                    buildChromosomeDisplay(false);
                }
            }
            else if (action == displayMutations)
                shouldDrawMutations_ = !shouldDrawMutations_;
            else if (action == displaySubstitutions)
                shouldDrawFixedSubstitutions_ = !shouldDrawFixedSubstitutions_;
            else if (action == displayGenomicElements)
                shouldDrawGenomicElements_ = !shouldDrawGenomicElements_;
            else if (action == displayRateMaps)
                shouldDrawRateMaps_ = !shouldDrawRateMaps_;
            else if (action == displayFrequencies)
                displayHaplotypes_ = false;
            else if (action == displayHaplotypes)
                displayHaplotypes_ = true;
            else
            {
                const std::map<slim_objectid_t,MutationType*> &muttypes = community->AllMutationTypes();
                
                if (action == displayAllMutations)
                    displayMuttypes_.clear();
                else if (action == selectNonneutralMutations)
                {
                    // - (IBAction)filterNonNeutral:(id)sender
                    displayMuttypes_.clear();
                    
                    for (auto muttype_iter : muttypes)
                    {
                        MutationType *muttype = muttype_iter.second;
                        slim_objectid_t muttype_id = muttype->mutation_type_id_;
                        
                        if ((muttype->dfe_type_ != DFEType::kFixed) || (muttype->dfe_parameters_[0] != 0.0))
                            displayMuttypes_.emplace_back(muttype_id);
                    }
                }
                else
                {
                    // - (IBAction)filterMutations:(id)sender
                    slim_objectid_t muttype_id = action->data().toInt();
                    auto present_iter = std::find(displayMuttypes_.begin(), displayMuttypes_.end(), muttype_id);
                    
                    if (present_iter == displayMuttypes_.end())
                    {
                        // this mut-type is not being displayed, so add it to our display list
                        displayMuttypes_.emplace_back(muttype_id);
                    }
                    else
                    {
                        // this mut-type is being displayed, so remove it from our display list
                        displayMuttypes_.erase(present_iter);
                    }
                }
            }
            
            emit needsRedisplay();
        }
    }
}

void QtSLiMChromosomeWidgetController::actionButtonRunMenu(QtSLiMPushButton *p_actionButton)
{
    QPoint mousePos = QCursor::pos();
    
    runChromosomeContextMenuAtPoint(mousePos);
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    p_actionButton->qtslimSetHighlight(false);
}


QtSLiMChromosomeWidget::QtSLiMChromosomeWidget(QWidget *p_parent, QtSLiMChromosomeWidgetController *controller, Species *displaySpecies, Qt::WindowFlags f)
#ifndef SLIM_NO_OPENGL
    : QOpenGLWidget(p_parent, f)
#else
    : QWidget(p_parent, f)
#endif
{
    controller_ = controller;
    setFocalDisplaySpecies(displaySpecies);
    
    // We support both OpenGL and non-OpenGL display, because some platforms seem
    // to have problems with OpenGL (https://github.com/MesserLab/SLiM/issues/462)
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::useOpenGLPrefChanged, this, [this]() { update(); });
}

QtSLiMChromosomeWidget::~QtSLiMChromosomeWidget()
{
    setDependentChromosomeView(nullptr);
	
    controller_ = nullptr;
}

void QtSLiMChromosomeWidget::setController(QtSLiMChromosomeWidgetController *controller)
{
    if (controller != controller_)
    {
        if (controller_)
            disconnect(controller_, &QtSLiMChromosomeWidgetController::needsRedisplay, this, nullptr);
        
        controller_ = controller;
        connect(controller, &QtSLiMChromosomeWidgetController::needsRedisplay, this, &QtSLiMChromosomeWidget::updateAfterTick);
    }
}

void QtSLiMChromosomeWidget::setFocalDisplaySpecies(Species *displaySpecies)
{
    // We can have no focal species (when coming out of the nib, in particular); in that case we display empty state
    if (displaySpecies && (displaySpecies->name_ != focalSpeciesName_))
    {
        // we've switched species, so we should remember the new one
        focalSpeciesName_ = displaySpecies->name_;
        
        // ... and reset to showing an overview of all the chromosomes
        focalChromosomeSymbol_ = "";
        
        update();
        updateDependentView();
    }
    else
    {
        // if displaySpecies is nullptr or unchanged, we just stick with our last remembered species
    }
}

Species *QtSLiMChromosomeWidget::focalDisplaySpecies(void)
{
    // We look up our focal species object by name every time, since keeping a pointer to it would be unsafe
    // Before initialize() is done species have not been created, so we return nullptr in that case
    if (focalSpeciesName_.length() == 0)
        return nullptr;
    
    if (controller_ && controller_->community() && (controller_->community()->Tick() >= 1))
        return controller_->community()->SpeciesWithName(focalSpeciesName_);
    
    return nullptr;
}

void QtSLiMChromosomeWidget::setFocalChromosome(Chromosome *chromosome)
{
    if (chromosome)
    {
        if (chromosome->Symbol() != focalChromosomeSymbol_)
        {
            // we've switched chromosomes, so remember the new one
            focalChromosomeSymbol_ = chromosome->Symbol();
            
            // ... and reset to the default selection
            setSelectedRange(QtSLiMRange(0, 0));
            
            // ... and if our new chromosome belongs to a different species, remember that
            if (chromosome->species_.name_ != focalSpeciesName_)
                focalSpeciesName_ = chromosome->species_.name_;
            
            update();
            updateDependentView();
        }
    }
    else
    {
        if (focalChromosomeSymbol_.length())
        {
            // we had a focal chromosome symbol, so reset to the overall view
            focalChromosomeSymbol_ = "";
            
            update();
            updateDependentView();
        }
    }
}

Chromosome *QtSLiMChromosomeWidget::focalChromosome(void)
{
    Species *focalSpecies = focalDisplaySpecies();
    
    if (focalSpecies)
    {
        if (focalChromosomeSymbol_.length())
        {
            Chromosome *chromosome = focalSpecies->ChromosomeFromSymbol(focalChromosomeSymbol_);
            
            if (isOverview_ && !chromosome)
            {
                // The focal chromosome apparently no longer exists, but we want to keep
                // trying to focus on it if it comes back (e.g., after a recycle), so we
                // do not reset or forget the focal chromosome symbol here; we only reset
                // the symbol to "" in setFocalDisplaySpecies() and setFocalChromosome().
                // However, if the focal species has chromosomes (and they don't match),
                // then apparently we're waiting for something that won't happen; give up
                // and switch.
                if (focalSpecies->Chromosomes().size() > 1)
                {
                    focalChromosomeSymbol_ = "";
                    return nullptr;
                }
                else if (focalSpecies->Chromosomes().size() == 1)
                {
                    Chromosome *chromosome = focalSpecies->Chromosomes()[0];
                    
                    focalChromosomeSymbol_ = chromosome->Symbol();
                    return chromosome;
                }
            }
            
            return chromosome;
        }
        else if (focalSpecies->Chromosomes().size() == 1)
        {
            // The species has just one chromosome, so there is no visual difference
            // between that chromosome being selected vs. not selected.  However, we
            // want to return that chromosome to the caller, so if it is not selected,
            // we fix that here and return it.
            Chromosome *chromosome = focalSpecies->Chromosomes()[0];
            
            focalChromosomeSymbol_ = chromosome->Symbol();
            return chromosome;
        }
    }
    
    return nullptr;
}

void QtSLiMChromosomeWidget::setDependentChromosomeView(QtSLiMChromosomeWidget *p_dependent_widget)
{
    if (dependentChromosomeView_ != p_dependent_widget)
    {
        dependentChromosomeView_ = p_dependent_widget;
        isOverview_ = (dependentChromosomeView_ ? true : false);
        showsTicks_ = !isOverview_;
        
        updateDependentView();
    }
}

void QtSLiMChromosomeWidget::updateDependentView(void)
{
    if (dependentChromosomeView_)
    {
        Chromosome *chromosome = focalChromosome();
        
        dependentChromosomeView_->setFocalChromosome(chromosome);
        
        if (chromosome)
            dependentChromosomeView_->setDisplayedRange(getSelectedRange(chromosome));
        else
            dependentChromosomeView_->setDisplayedRange(QtSLiMRange(0, 0)); // display entirely
        
        dependentChromosomeView_->stateChanged();
    }
}

void QtSLiMChromosomeWidget::stateChanged(void)
{
    update();
}

void QtSLiMChromosomeWidget::updateAfterTick(void)
{
    // overview chromosomes don't need to update all the time, since their display doesn't change
    if (!isOverview_)
        stateChanged();
}

#ifndef SLIM_NO_OPENGL
void QtSLiMChromosomeWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
}

void QtSLiMChromosomeWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    
	// Update the projection
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}
#endif

QRect QtSLiMChromosomeWidget::rectEncompassingBaseToBase(slim_position_t startBase, slim_position_t endBase, QRect interiorRect, QtSLiMRange displayedRange)
{
	double startFraction = (startBase - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length);
	double leftEdgeDouble = interiorRect.left() + startFraction * interiorRect.width();
	double endFraction = (endBase + 1 - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length);
	double rightEdgeDouble = interiorRect.left() + endFraction * interiorRect.width();
	int leftEdge, rightEdge;
	
	if (rightEdgeDouble - leftEdgeDouble > 1.0)
	{
		// If the range spans a width of more than one pixel, then use the maximal pixel range
		leftEdge = static_cast<int>(floor(leftEdgeDouble));
		rightEdge = static_cast<int>(ceil(rightEdgeDouble));
	}
	else
	{
		// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the left-right positions span a pixel boundary
		leftEdge = static_cast<int>(floor(leftEdgeDouble));
		rightEdge = leftEdge + 1;
	}
	
	return QRect(leftEdge, interiorRect.top(), rightEdge - leftEdge, interiorRect.height());
}

slim_position_t QtSLiMChromosomeWidget::baseForPosition(double position, QRect interiorRect, QtSLiMRange displayedRange)
{
	double fraction = (position - interiorRect.left()) / interiorRect.width();
	slim_position_t base = static_cast<slim_position_t>(floor(fraction * (displayedRange.length + 1) + displayedRange.location));
	
	return base;
}

QRect QtSLiMChromosomeWidget::getContentRect(void)
{
    QRect bounds = rect();
	
	// The height gets adjusted because our "content rect" does not include the space for selection knobs below
    // (for the overview) or for tick marks and labels (for the zoomed view).  Note that SLiMguiLegacy has a two-
    // pixel margin on the left and right of the chromosome view, to avoid clipping the selection knobs, but that
    // is a bit harder to do in Qt since the UI layout is trickier, so we just let the knobs clip; it's fine.
    int bottomMargin = (isOverview_ ? (selectionKnobSize+1) : heightForTicks);
    
    if (!isOverview_ && !showsTicks_)
        bottomMargin = 0;
    
    return QRect(bounds.left(), bounds.top(), bounds.width(), bounds.height() - bottomMargin);
}

QtSLiMRange QtSLiMChromosomeWidget::getSelectedRange(Chromosome *chromosome)
{
    if (hasSelection_ && chromosome && (chromosome == focalChromosome()))
	{
		return QtSLiMRange(selectionFirstBase_, selectionLastBase_ - selectionFirstBase_ + 1);	// number of bases encompassed; a selection from x to x encompasses 1 base
	}
    else if (chromosome)
	{
		slim_position_t chromosomeLastPosition = chromosome->last_position_;
		
		return QtSLiMRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
	}
    else
    {
        return QtSLiMRange(0, 0);
    }
}

void QtSLiMChromosomeWidget::setSelectedRange(QtSLiMRange p_selectionRange)
{
    if (isOverview_ && (p_selectionRange.length >= 1))
	{
		selectionFirstBase_ = static_cast<slim_position_t>(p_selectionRange.location);
		selectionLastBase_ = static_cast<slim_position_t>(p_selectionRange.location + p_selectionRange.length) - 1;
		hasSelection_ = true;
		
		// Save the selection for restoring across recycles, etc.
		savedSelectionFirstBase_ = selectionFirstBase_;
		savedSelectionLastBase_ = selectionLastBase_;
		savedHasSelection_ = hasSelection_;
	}
	else if (hasSelection_)
	{
		hasSelection_ = false;
		
		// Save the selection for restoring across recycles, etc.
		savedHasSelection_ = hasSelection_;
	}
	else
	{
		// Save the selection for restoring across recycles, etc.
		savedHasSelection_ = false;
		
		return;
	}
	
	// Our selection changed, so update and post a change notification
    update();
    
    if (isOverview_ && dependentChromosomeView_)
        updateDependentView();
}

void QtSLiMChromosomeWidget::restoreLastSelection(void)
{
    if (isOverview_ && savedHasSelection_)
	{
		selectionFirstBase_ = savedSelectionFirstBase_;
		selectionLastBase_ = savedSelectionLastBase_;
		hasSelection_ = savedHasSelection_;
	}
	else if (hasSelection_)
	{
		hasSelection_ = false;
	}
	
	// Our selection changed, so update and post a change notification
	update();
    
    // We want to always post the notification, to make sure updating happens correctly;
    // this ensures that correct ticks marks get drawn after a recycle, etc.
    if (isOverview_ && dependentChromosomeView_)
        updateDependentView();
}

QtSLiMRange QtSLiMChromosomeWidget::getDisplayedRange(Chromosome *chromosome)
{
    if (isOverview_)
    {
        // the overview always displays the whole length
        slim_position_t chromosomeLastPosition = chromosome->last_position_;
        
        return QtSLiMRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
    }
    else if (!chromosome || (displayedRange_.length == 0))
    {
        // the detail view displays the entire length unless a specific displayed range is set
        slim_position_t chromosomeLastPosition = chromosome->last_position_;
        
        return QtSLiMRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
    }
    else
    {
		return displayedRange_;
    }
}

void QtSLiMChromosomeWidget::setDisplayedRange(QtSLiMRange p_displayedRange)
{
    displayedRange_ = p_displayedRange;
    update();
}

void QtSLiMChromosomeWidget::setShowsTicks(bool p_showTicks)
{
    if (p_showTicks != showsTicks_)
    {
        showsTicks_ = p_showTicks;
        update();
    }
}

#ifndef SLIM_NO_OPENGL
void QtSLiMChromosomeWidget::paintGL()
#else
void QtSLiMChromosomeWidget::paintEvent(QPaintEvent * /* p_paint_event */)
#endif
{
    QPainter painter(this);
    bool inDarkMode = QtSLiMInDarkMode();
    
    painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    //painter.fillRect(rect(), Qt::red);
    
    painter.setPen(Qt::black);      // make sure we have our default color of black, since Qt apparently does not guarantee that
    
    Species *displaySpecies = focalDisplaySpecies();
    bool ready = (isEnabled() && controller_ && !controller_->invalidSimulation() && (displaySpecies != nullptr));
    QRect contentRect = getContentRect();
	QRect interiorRect = contentRect.marginsRemoved(QMargins(1, 1, 1, 1));
    
    // if the simulation is at tick 0, it is not ready
	if (ready)
        if (controller_->community()->Tick() == 0)
			ready = false;
	
    if (ready)
    {
        if (isOverview_)
        {
            drawOverview(displaySpecies, painter);
        }
        else
        {
            Chromosome *chromosome = focalChromosome();
            
            if (!chromosome)
            {
                // display all chromosomes simultaneously
                drawFullGenome(displaySpecies, painter);
            }
            else
            {
                // display one chromosome in the regular way
                QtSLiMRange displayedRange = getDisplayedRange(chromosome);
                
                // draw ticks at bottom of content rect
                if (showsTicks_)
                    drawTicksInContentRect(contentRect, displaySpecies, displayedRange, painter);
                    
                    // do the core drawing, with or without OpenGL according to user preference
#ifndef SLIM_NO_OPENGL
                if (QtSLiMPreferencesNotifier::instance().useOpenGLPref())
                {
                    painter.beginNativePainting();
                    glDrawRect(contentRect, displaySpecies, chromosome);
                    painter.endNativePainting();
                }
                else
#endif
                {
                    qtDrawRect(contentRect, displaySpecies, chromosome, painter);
                }
                
                // frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
                QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
            }
        }
    }
    else
    {
        // erase the content area itself
        painter.fillRect(interiorRect, QtSLiMColorWithWhite(inDarkMode ? 0.118 : 0.9, 1.0));
        
        // frame
        QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.77, 1.0), painter);
    }
}

void QtSLiMChromosomeWidget::drawOverview(Species *displaySpecies, QPainter &painter)
{
    // the overview draws all of the chromosomes showing genomic elements; always with Qt, not GL
    Chromosome *focalChrom = focalChromosome();
    QRect contentRect = getContentRect();
    bool inDarkMode = QtSLiMInDarkMode();
    
    if (!displaySpecies->HasGenetics())
    {
        QRect interiorRect = contentRect.marginsRemoved(QMargins(1, 1, 1, 1));
        
        painter.fillRect(interiorRect, QtSLiMColorWithWhite(inDarkMode ? 0.118 : 0.9, 1.0));
        QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.77, 1.0), painter);
        return;
    }
    
    const std::vector<Chromosome *> &chromosomes = displaySpecies->Chromosomes();
    int chromosomeCount = (int)chromosomes.size();
    int64_t availableWidth = contentRect.width() - (chromosomeCount * 2) - ((chromosomeCount - 1) * spaceBetweenChromosomes);
    int64_t totalLength = 0;
    
    // after a delay, we show chromosome numbers unless we're tracking or have a single-chromosome model
    bool showChromosomeNumbers = (showChromosomeNumbers_ && !isTracking_ && (chromosomeCount > 1));
    
    for (Chromosome *chrom : chromosomes)
    {
        slim_position_t chromLength = (chrom->last_position_ + 1);
        
        totalLength += chromLength;
    }
    
    if (showChromosomeNumbers)
    {
        painter.save();
        
        static QFont *tickFont = nullptr;
        
        if (!tickFont)
        {
            tickFont = new QFont();
#ifdef __linux__
            tickFont->setPointSize(8);
#else
            tickFont->setPointSize(10);
#endif
        }
        painter.setFont(*tickFont);
    }
    
    int64_t remainingLength = totalLength;
    int leftPosition = contentRect.left();
    
    for (Chromosome *chrom : chromosomes)
    {
        double scale = (double)availableWidth / remainingLength;
        slim_position_t chromLength = (chrom->last_position_ + 1);
        int width = (int)round(chromLength * scale);
        int paddedWidth = 2 + width;
        QRect chromContentRect(leftPosition, contentRect.top(), paddedWidth, contentRect.height());
        QRect chromInteriorRect = chromContentRect.marginsRemoved(QMargins(1, 1, 1, 1));
        QtSLiMRange displayedRange = getDisplayedRange(chrom);
        
        if (showChromosomeNumbers)
        {
            painter.fillRect(chromInteriorRect, Qt::white);
            
            const std::string &symbol = chrom->Symbol();
            QString symbolLabel = QString::fromStdString(symbol);
            QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, symbolLabel);
            double labelWidth = labelBoundingRect.width();
            
            // display the chromosome symbol only if there is space for it
            if (labelWidth < chromInteriorRect.width())
            {
                int symbolLabelX = static_cast<int>(round(chromContentRect.center().x())) + 1;
                int symbolLabelY = static_cast<int>(round(chromContentRect.center().y())) + 7;
                int textFlags = (Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignBottom | Qt::AlignHCenter);
                
                painter.drawText(QRect(symbolLabelX, symbolLabelY, 0, 0), textFlags, symbolLabel);
            }
        }
        else
        {
            painter.fillRect(chromInteriorRect, Qt::black);
            
            qtDrawGenomicElements(chromInteriorRect, chrom, displayedRange, painter);                    
        }
        
        if (chrom == focalChrom)
        {
            if (hasSelection_)
            {
                // overlay the selection last, since it bridges over the frame; when showing chromosome numbers
                // in light mode, drawing the interior shadow looks better because of the white background
                QtSLiMFrameRect(chromContentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
                overlaySelection(chromInteriorRect, displayedRange, /* drawInteriorShadow */ showChromosomeNumbers && !inDarkMode, painter);
            }
            else if (chromosomes.size() > 1)
            {
                // the selected chromosome gets a heavier frame, if we have more than one chromosome
                QtSLiMFrameRect(chromContentRect, QtSLiMColorWithWhite(inDarkMode ? 0.0 : 0.4, 1.0), painter);
            }
            else
            {
                QtSLiMFrameRect(chromContentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
            }
        }
        else
        {
            if ((chromosomes.size() > 1) && focalChrom)
            {
                // with more than one chromosome, chromosomes other than the focal chromosome get washed out
                painter.fillRect(chromInteriorRect, QtSLiMColorWithWhite(inDarkMode ? 0.0 : 1.0, inDarkMode ? 0.50 : 0.60));
                QtSLiMFrameRect(chromContentRect, QtSLiMColorWithWhite(inDarkMode ? 0.1 : 0.8, 1.0), painter);
            }
            else
            {
                QtSLiMFrameRect(chromContentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
            }
        }
        
        leftPosition += (paddedWidth + spaceBetweenChromosomes);
        availableWidth -= width;
        remainingLength -= chromLength;
    }
    
    if (showChromosomeNumbers)
    {
        painter.restore();
    }
}

void QtSLiMChromosomeWidget::drawFullGenome(Species *displaySpecies, QPainter &painter)
{
    // this is similar to drawOverview(), but shows the detail view and can use GL
    QRect contentRect = getContentRect();
    bool inDarkMode = QtSLiMInDarkMode();
    const std::vector<Chromosome *> &chromosomes = displaySpecies->Chromosomes();
    int chromosomeCount = (int)chromosomes.size();
    int64_t availableWidth = contentRect.width() - (chromosomeCount * 2) - ((chromosomeCount - 1) * spaceBetweenChromosomes);
    int64_t totalLength = 0;
    
    if (!displaySpecies->HasGenetics())
    {
        QRect interiorRect = contentRect.marginsRemoved(QMargins(1, 1, 1, 1));
        
        painter.fillRect(interiorRect, QtSLiMColorWithWhite(inDarkMode ? 0.118 : 0.9, 1.0));
        QtSLiMFrameRect(contentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.77, 1.0), painter);
        return;
    }
    
    for (Chromosome *chrom : chromosomes)
    {
        slim_position_t chromLength = (chrom->last_position_ + 1);
        
        totalLength += chromLength;
    }
    
    int64_t remainingLength = totalLength;
    int leftPosition = contentRect.left();
    
    for (Chromosome *chrom : chromosomes)
    {
        // display one chromosome in the regular way
        double scale = (double)availableWidth / remainingLength;
        slim_position_t chromLength = (chrom->last_position_ + 1);
        int width = (int)round(chromLength * scale);
        int paddedWidth = 2 + width;
        QRect chromContentRect(leftPosition, contentRect.top(), paddedWidth, contentRect.height());
        QRect chromInteriorRect = chromContentRect.marginsRemoved(QMargins(1, 1, 1, 1));
        slim_position_t chromosomeLastPosition = chrom->last_position_;
        QtSLiMRange displayedRange = QtSLiMRange(0, chromosomeLastPosition + 1);    // chromosomeLastPosition + 1 bases are encompassed
        
        painter.fillRect(chromInteriorRect, Qt::black);
        
        // draw ticks at bottom of content rect
        if (showsTicks_)
            drawTicksInContentRect(chromContentRect, displaySpecies, displayedRange, painter);
        
        // do the core drawing, with or without OpenGL according to user preference
#ifndef SLIM_NO_OPENGL
        if (QtSLiMPreferencesNotifier::instance().useOpenGLPref())
        {
            painter.beginNativePainting();
            glDrawRect(chromContentRect, displaySpecies, chrom);
            painter.endNativePainting();
        }
        else
#endif
        {
            qtDrawRect(chromContentRect, displaySpecies, chrom, painter);
        }
        
        // frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
        QtSLiMFrameRect(chromContentRect, QtSLiMColorWithWhite(inDarkMode ? 0.067 : 0.6, 1.0), painter);
        
        leftPosition += (paddedWidth + spaceBetweenChromosomes);
        availableWidth -= width;
        remainingLength -= chromLength;
    }
}

void QtSLiMChromosomeWidget::drawTicksInContentRect(QRect contentRect, __attribute__((__unused__)) Species *displaySpecies, QtSLiMRange displayedRange, QPainter &painter)
{
    bool inDarkMode = QtSLiMInDarkMode();
	QRect interiorRect = contentRect.marginsRemoved(QMargins(1, 1, 1, 1));
	int64_t lastTickIndex = numberOfTicksPlusOne;
	
    painter.save();
    painter.setPen(inDarkMode ? Qt::white : Qt::black);
    
	// Display fewer ticks when we are displaying a very small number of positions
	lastTickIndex = std::min(lastTickIndex, (displayedRange.length + 1) / 3);
	
	double tickIndexDivisor = ((lastTickIndex == 0) ? 1.0 : static_cast<double>(lastTickIndex));		// avoid a divide by zero when we are displaying a single site
    static QFont *tickFont = nullptr;
    
    if (!tickFont)
    {
        tickFont = new QFont();
#ifdef __linux__
        tickFont->setPointSize(7);
#else
        tickFont->setPointSize(9);
#endif
    }
    painter.setFont(*tickFont);
    
    QFontMetricsF fontMetrics(*tickFont);
    
    if (displayedRange.length == 0)
	{
		// Handle the "no genetics" case separately
		if (!isOverview_)
		{
            QString tickLabel("no genetics");
            int tickLabelX = static_cast<int>(floor(contentRect.left() + contentRect.width() / 2.0));
            int tickLabelY = contentRect.bottom() + (2 + 13);
            int textFlags = (Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignBottom | Qt::AlignHCenter);
            
            painter.drawText(QRect(tickLabelX, tickLabelY, 0, 0), textFlags, tickLabel);
		}
		
        painter.restore();
		return;
	}
    
    int rightmostNotDrawn = interiorRect.right();   // used to avoid overlapping labels
    bool useScientificNotation = false;             // the rightmost tick determines this since it has the largest tickBase
    
    // Draw tick marks and tick labels; we go from the right backwards because we want to at least fit the rightmost tick label if we can
    // FIXME even better would be to do lastTickIndex, then 0, then the middle, then the rest, but we'd have to track leftmostNotDrawn as well...
	for (int tickIndex = lastTickIndex; tickIndex >= 0; --tickIndex)
	{
        // first figure out the tick position
        slim_position_t tickBase = static_cast<slim_position_t>(displayedRange.location) + static_cast<slim_position_t>(ceil((displayedRange.length - 1) * (tickIndex / tickIndexDivisor)));	// -1 because we are choosing an in-between-base position that falls, at most, to the left of the last base
        QRect tickRect = rectEncompassingBaseToBase(tickBase, tickBase, interiorRect, displayedRange);
        
        tickRect.setHeight(tickLength);
        tickRect.moveBottom(contentRect.bottom() + tickLength);
        
        // figure out the label's alignment relative to the tick
        bool forceCenteredLabel = (tickRect.width() > 50);      // with wide ticks, just center all labels; there's room for lots of digits here
        Qt::AlignmentFlag labelAlignment;
        int tickLabelX;
        
        if ((tickIndex == lastTickIndex) && !forceCenteredLabel)
        {
            labelAlignment = Qt::AlignRight;
            tickLabelX = tickRect.right() + 2;
        }
        else if ((tickIndex == 0) && !forceCenteredLabel)
        {
            labelAlignment = Qt::AlignLeft;
            tickLabelX = tickRect.left() - 1;
        }
        else
        {
            labelAlignment = Qt::AlignHCenter;
            tickLabelX = static_cast<int>(floor(tickRect.left() + tickRect.width() / 2.0)) + 1;
        }
        
        // make the label
        QString tickLabel;
        
        if (tickBase >= 1e9)
            useScientificNotation = true;
        
        if (useScientificNotation && (tickBase != 0))
        {
            tickLabel = QString::asprintf("%.4e", static_cast<double>(tickBase));		// scientific notation above a threshold
            
            // remove cruft around the exponential; we want "1.5000e+09" -> "1.5e9", "1.0000e+09" -> "1.0e9", etc.
            tickLabel.replace(".0000e", ".0e");
            tickLabel.replace(".000e", ".0e");
            tickLabel.replace("000e", "e");     // didn't get replaced by the previous line, so it must follow a non-zero digit
            tickLabel.replace(".00e", ".0e");
            tickLabel.replace("00e", "e");      // didn't get replaced by the previous line, so it must follow a non-zero digit
            tickLabel.replace("e+0", "e");
            tickLabel.replace("e+", "e");
        }
        else
            QTextStream(&tickLabel) << static_cast<int64_t>(tickBase);
        
        // measure it
#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
        double tickLabelWidth = fontMetrics.width(tickLabel);               // deprecated in 5.11
#else
        double tickLabelWidth = fontMetrics.horizontalAdvance(tickLabel);   // added in Qt 5.11
#endif
        
        int labelLeftEdge, labelRightEdge;
        
        if (labelAlignment == Qt::AlignRight)
        {
            labelLeftEdge = tickLabelX - tickLabelWidth;
            labelRightEdge = tickLabelX;
        }
        else if (labelAlignment == Qt::AlignLeft)
        {
            labelLeftEdge = tickLabelX;
            labelRightEdge = tickLabelX + tickLabelWidth;
        }
        else    // Qt::AlignHCenter
        {
            labelLeftEdge = tickLabelX - (int)std::ceil(tickLabelWidth / 2.0);
            labelRightEdge = tickLabelX + (int)std::ceil(tickLabelWidth / 2.0);
        }
        
        // decide whether the label fits
        bool drawLabel = true;
        
        if ((tickIndex > 0) && (labelLeftEdge < interiorRect.left()))
            drawLabel = false;
        else if ((tickIndex < lastTickIndex) && (labelRightEdge + 5 > rightmostNotDrawn))
            drawLabel = false;
        
        if (!drawLabel && (tickIndex == lastTickIndex))                         // if the rightmost label doesn't fit, skip all ticks and labels
            break;
        if (!drawLabel && (tickIndex != 0) && (tickIndex != lastTickIndex))     // skip interior tick marks if we skip their label
            continue;
        
        // draw a tick for it; if we are displaying a single site or two sites, make a tick mark one pixel wide; a very wide one looks weird
        if (displayedRange.length <= 2)
        {
            tickRect.setLeft(static_cast<int>(floor(tickRect.left() + tickRect.width() / 2.0 - 0.5)));
            tickRect.setWidth(1);
        }
        
        painter.fillRect(tickRect, inDarkMode ? QColor(10, 10, 10, 255) : QColor(127, 127, 127, 255));  // in dark mode, 17 matches the frame, but is too light
        
        // if we decided to draw the tick even though the label doesn't fit, now skip the label
        if (!drawLabel)
            continue;
        
        // and then draw the tick label
        int tickLabelY = contentRect.bottom() + (tickLength + 13);
        int textFlags = (Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignBottom | labelAlignment);
        
        painter.drawText(QRect(tickLabelX, tickLabelY, 0, 0), textFlags, tickLabel);
        
        // keep track of where we have drawn text, to avoid overlap
        rightmostNotDrawn = labelLeftEdge;
	}
    
    painter.restore();
}

void QtSLiMChromosomeWidget::updateDisplayedMutationTypes(Species *displaySpecies)
{
    // We use a flag in MutationType to indicate whether we're drawing that type or not; we update those flags here,
    // before every drawing of mutations, from the vector of mutation type IDs that we keep internally
    if (controller_)
    {
        if (displaySpecies)
        {
            std::map<slim_objectid_t,MutationType*> &muttypes = displaySpecies->mutation_types_;
            std::vector<slim_objectid_t> &displayTypes = displayMuttypes();
            
            for (auto muttype_iter : muttypes)
            {
                MutationType *muttype = muttype_iter.second;
                
                if (displayTypes.size())
                {
                    slim_objectid_t muttype_id = muttype->mutation_type_id_;
                    
                    muttype->mutation_type_displayed_ = (std::find(displayTypes.begin(), displayTypes.end(), muttype_id) != displayTypes.end());
                }
                else
                {
                    muttype->mutation_type_displayed_ = true;
                }
            }
        }
    }
}

void QtSLiMChromosomeWidget::overlaySelection(QRect interiorRect, QtSLiMRange displayedRange, bool drawInteriorShadow, QPainter &painter)
{
	if (hasSelection_)
	{
        // wash out the exterior of the selection
        bool inDarkMode = QtSLiMInDarkMode();
        
        if (selectionFirstBase_ > 0)
        {
            QRect leftOfSelectionRect = rectEncompassingBaseToBase(0, selectionFirstBase_ - 1, interiorRect, displayedRange);
            
            painter.fillRect(leftOfSelectionRect, QtSLiMColorWithWhite(inDarkMode ? 0.0 : 1.0, inDarkMode ? 0.50 : 0.60));
        }
        if (selectionLastBase_ < displayedRange.length - 1)
        {
            QRect rightOfSelectionRect = rectEncompassingBaseToBase(selectionLastBase_ + 1, displayedRange.length - 1, interiorRect, displayedRange);
            
            painter.fillRect(rightOfSelectionRect, QtSLiMColorWithWhite(inDarkMode ? 0.0 : 1.0, inDarkMode ? 0.50 : 0.60));
        }
        
		// draw a bar at the start and end of the selection
        QRect selectionRect = rectEncompassingBaseToBase(selectionFirstBase_, selectionLastBase_, interiorRect, displayedRange);
		QRect selectionStartBar1 = QRect(selectionRect.left() - 1, interiorRect.top(), 1, interiorRect.height());
		QRect selectionStartBar2 = QRect(selectionRect.left(), interiorRect.top(), 1, interiorRect.height() + 5);
		QRect selectionStartBar3 = QRect(selectionRect.left() + 1, interiorRect.top(), 1, interiorRect.height());
		QRect selectionEndBar1 = QRect(selectionRect.left() + selectionRect.width() - 2, interiorRect.top(), 1, interiorRect.height());
		QRect selectionEndBar2 = QRect(selectionRect.left() + selectionRect.width() - 1, interiorRect.top(), 1, interiorRect.height() + 5);
		QRect selectionEndBar3 = QRect(selectionRect.left() + selectionRect.width(), interiorRect.top(), 1, interiorRect.height());
		
        painter.fillRect(selectionStartBar1, QtSLiMColorWithWhite(1.0, 0.15));
        if (drawInteriorShadow)
            painter.fillRect(selectionEndBar1, QtSLiMColorWithWhite(1.0, 0.15));
        
        painter.fillRect(selectionStartBar2, inDarkMode ? QtSLiMColorWithWhite(0.8, 1.0) : Qt::black);
        painter.fillRect(selectionEndBar2, inDarkMode ? QtSLiMColorWithWhite(0.8, 1.0) : Qt::black);
		
        if (drawInteriorShadow)
            painter.fillRect(selectionStartBar3, QtSLiMColorWithWhite(0.0, 0.30));
        painter.fillRect(selectionEndBar3, QtSLiMColorWithWhite(0.0, 0.30));
        
		// draw a ball at the end of each bar
        // FIXME this doesn't look quite as nice as SLiMgui, because QPainter doesn't antialias
        // also we can get clipped by one pixel at the edge of the view; subtle but imperfect
		QRect selectionStartBall = QRect(selectionRect.left() - selectionKnobSizeExtension, interiorRect.bottom() + (selectionKnobSize - 2), selectionKnobSize, selectionKnobSize);
		QRect selectionEndBall = QRect(selectionRect.left() + selectionRect.width() - (selectionKnobSizeExtension + 1), interiorRect.bottom() + (selectionKnobSize - 2), selectionKnobSize, selectionKnobSize);
		
        painter.save();
        painter.setPen(Qt::NoPen);
        
        painter.setBrush(inDarkMode ? QtSLiMColorWithWhite(0.65, 1.0) : Qt::black);	// outline
		painter.drawEllipse(selectionStartBall);
		painter.drawEllipse(selectionEndBall);
		
        painter.setBrush(QtSLiMColorWithWhite(0.3, 1.0));	// interior
		painter.drawEllipse(selectionStartBall.adjusted(1, 1, -1, -1));
		painter.drawEllipse(selectionEndBall.adjusted(1, 1, -1, -1));
		
		painter.setBrush(QtSLiMColorWithWhite(1.0, 0.5));	// highlight
		painter.drawEllipse(selectionStartBall.adjusted(1, 1, -2, -2));
		painter.drawEllipse(selectionEndBall.adjusted(1, 1, -2, -2));
        
        painter.restore();
    }
}

Chromosome *QtSLiMChromosomeWidget::_findFocalChromosomeForTracking(QMouseEvent *p_event)
{
    // this hit-tracks the same layout that drawOverview() displays
    QPoint curPoint = p_event->pos();
    QRect overallRect = rect();
    QRect contentRect = getContentRect();
    Species *displaySpecies = focalDisplaySpecies();
    const std::vector<Chromosome *> &chromosomes = displaySpecies->Chromosomes();
    int chromosomeCount = (int)chromosomes.size();
    int64_t availableWidth = contentRect.width() - (chromosomeCount * 2) - ((chromosomeCount - 1) * spaceBetweenChromosomes);
    int64_t totalLength = 0;
    
    for (Chromosome *chrom : chromosomes)
    {
        slim_position_t chromLength = (chrom->last_position_ + 1);
        
        totalLength += chromLength;
    }
    
    int64_t remainingLength = totalLength;
    int leftPosition = contentRect.left();
    
    // note that we hit-test against the overall frames of the chromosomes (including the margin
    // at the bottom for selection knobs), but set contentRectForTrackedChromosome_ based on the
    // content rect for the chromosome (excluding that margin); see mousePressEvent() for why.
    for (Chromosome *chrom : chromosomes)
    {
        double scale = (double)availableWidth / remainingLength;
        slim_position_t chromLength = (chrom->last_position_ + 1);
        int width = (int)round(chromLength * scale);
        int paddedWidth = 2 + width;
        QRect chromOverallFrame(leftPosition, overallRect.top(), paddedWidth, overallRect.height());
        
        if (chromOverallFrame.contains(curPoint))
        {
            QRect chromContentRect(leftPosition, contentRect.top(), paddedWidth, contentRect.height());
            
            contentRectForTrackedChromosome_ = chromContentRect;
            return chrom;
        }
        
        leftPosition += (paddedWidth + spaceBetweenChromosomes);
        availableWidth -= width;
        remainingLength -= chromLength;
    }
    
    return nullptr;
}

void QtSLiMChromosomeWidget::mousePressEvent(QMouseEvent *p_event)
{
    Species *displaySpecies = focalDisplaySpecies();
	bool ready = (isOverview_ && isEnabled() && !controller_->invalidSimulation() && (displaySpecies != nullptr));
	
	// if the simulation is at tick 0, it is not ready
	if (ready)
        if (controller_->community()->Tick() == 0)
			ready = false;
	
	if (ready)
	{
        // find which chromosome was clicked in; this sets contentRectForTrackedChromosome_ to the content rect of that chromosome
        // note that it hit-tests aginst the overall chromosome view, including the selection knob margin, though
        Chromosome *hitChromosome = _findFocalChromosomeForTracking(p_event);
        
        simpleClickInFocalChromosome_ = false;     // only true in the one case set below
        
        // if the click was not in a chromosome (like in the gap between them), just return with no effect
        if (!hitChromosome)
            return;
        
        QRect contentRect = contentRectForTrackedChromosome_;
        QRect interiorRect = contentRect.marginsRemoved(QMargins(1, 1, 1, 1));
        QtSLiMRange displayedRange = getDisplayedRange(hitChromosome);
        QPoint curPoint = p_event->pos();
        
        // check for a hit in one of our selection handles
        if (hasSelection_ && (hitChromosome == focalChromosome()))
		{
			QRect selectionRect = rectEncompassingBaseToBase(selectionFirstBase_, selectionLastBase_, interiorRect, displayedRange);
			int leftEdge = selectionRect.left();
			int rightEdge = selectionRect.left() + selectionRect.width() - 1;	// -1 to be on the left edge of the right-edge pixel strip
			QRect leftSelectionBar = QRect(leftEdge - 2, selectionRect.top() - 1, 5, selectionRect.height() + 2);
			QRect leftSelectionKnob = QRect(leftEdge - (selectionKnobSizeExtension + 1), selectionRect.bottom() + (selectionKnobSize - 3), (selectionKnobSizeExtension + 1) * 2 + 1, selectionKnobSize + 2);
			QRect rightSelectionBar = QRect(rightEdge - 2, selectionRect.top() - 1, 5, selectionRect.height() + 2);
			QRect rightSelectionKnob = QRect(rightEdge - (selectionKnobSizeExtension + 1), selectionRect.bottom() + (selectionKnobSize - 3), (selectionKnobSizeExtension + 1) * 2 + 1, selectionKnobSize + 2);
			
			if (leftSelectionBar.contains(curPoint) || leftSelectionKnob.contains(curPoint))
			{
				isTracking_ = true;
                movedSufficiently_ = true;  // a hit in a selection bar is unambiguously a drag
				trackingXAdjust_ = (curPoint.x() - leftEdge) - 1;		// I'm not sure why the -1 is needed, but it is...
				trackingStartBase_ = selectionLastBase_;	// we're dragging the left knob, so the right knob is the tracking anchor
				trackingLastBase_ = baseForPosition(curPoint.x() - trackingXAdjust_, interiorRect, displayedRange);	// instead of selectionFirstBase, so the selection does not change at all if the mouse does not move
				
				mouseMoveEvent(p_event);	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
			else if (rightSelectionBar.contains(curPoint) || rightSelectionKnob.contains(curPoint))
			{
				isTracking_ = true;
                movedSufficiently_ = true;  // a hit in a selection bar is unambiguously a drag
				trackingXAdjust_ = (curPoint.x() - rightEdge);
				trackingStartBase_ = selectionFirstBase_;	// we're dragging the right knob, so the left knob is the tracking anchor
				trackingLastBase_ = baseForPosition(curPoint.x() - trackingXAdjust_, interiorRect, displayedRange);	// instead of selectionLastBase, so the selection does not change at all if the mouse does not move
				
				mouseMoveEvent(p_event);	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
		}
        
        // _findFocalChromosomeForTracking() will return a hit anywhere in the overall chromosome view, so that we can test for hits
        // in the selection knobs above; but from this point forward, we only want to handle hits that are actually in the content area
        if (!contentRect.contains(curPoint))
            return;
        
        // our behavior depends on whether the hit chromosome was already the focal chromosome
        if ((hitChromosome == focalChromosome()) && !hasSelection_)
        {
            // if the click was in the currently selected chromosome, and there is presently no selection, remember
            // that fact; if we get a mouse-up without a selection being dragged out, we will deselect completely
            // (if we presently have a selection, the click removes the selection, but does not deselect completely)
            simpleClickInFocalChromosome_ = true;
            update();
        }
        else if (hitChromosome != focalChromosome())
        {
            // given that it wasn't a hit in a selection handle, we now switch to the chromosome that was clicked in;
            // other kinds of clicks change the focal chromosome to the one hit by the click
            setFocalChromosome(hitChromosome);
            update();
        }
        
		// option-clicks just set the selection to the clicked genomic element, no questions asked
        // tracking does not continue beyond this step, since we don't set isTracking_ = true
        if (p_event->modifiers() & Qt::AltModifier)
		{
            slim_position_t clickedBase = baseForPosition(curPoint.x(), interiorRect, displayedRange);
            QtSLiMRange selectionRange = QtSLiMRange(0, 0);
            GenomicElement *genomicElement = hitChromosome->ElementForPosition(clickedBase);
            
            if (genomicElement)
            {
                slim_position_t startPosition = genomicElement->start_position_;
                slim_position_t endPosition = genomicElement->end_position_;
                selectionRange = QtSLiMRange(startPosition, endPosition - startPosition + 1);
            }
            
            mouseInsideCounter_++;  // prevent a flip to displaying chromosome numbers
            simpleClickInFocalChromosome_ = false;
            
            setSelectedRange(selectionRange);
            return;
        }
        
        // otherwise we have an ordinary click, selecting a chromosome and perhaps dragging out a selection
        {
            isTracking_ = true;
            movedSufficiently_ = false;     // require a movement threshold before beginning to drag
            initialMouseX = curPoint.x();
            trackingStartBase_ = baseForPosition(curPoint.x(), interiorRect, displayedRange);
            trackingLastBase_ = trackingStartBase_;
            trackingXAdjust_ = 0;
            
            // We start off with no selection, and wait for the user to drag out a selection
			if (hasSelection_)
			{
				hasSelection_ = false;
				
				// Save the selection for restoring across recycles, etc.
				savedHasSelection_ = hasSelection_;
				
				update();
                updateDependentView();
			}
        }
	}
}

void QtSLiMChromosomeWidget::_mouseTrackEvent(QMouseEvent *p_event)
{
    QRect contentRect = contentRectForTrackedChromosome_;
    QRect interiorRect = contentRect.marginsRemoved(QMargins(1, 1, 1, 1));
    QtSLiMRange displayedRange = getDisplayedRange(focalChromosome());
    QPoint curPoint = p_event->pos();
	
	QPoint correctedPoint = QPoint(curPoint.x() - trackingXAdjust_, curPoint.y());
	slim_position_t trackingNewBase = baseForPosition(correctedPoint.x(), interiorRect, displayedRange);
	bool selectionChanged = false;
	
	if (trackingNewBase != trackingLastBase_)
	{
		trackingLastBase_ = trackingNewBase;
		
		slim_position_t trackingLeftBase = trackingStartBase_, trackingRightBase = trackingLastBase_;
		
		if (trackingLeftBase > trackingRightBase)
		{
			trackingLeftBase = trackingLastBase_;
			trackingRightBase = trackingStartBase_;
		}
		
		if (trackingLeftBase <= static_cast<slim_position_t>(displayedRange.location))
			trackingLeftBase = static_cast<slim_position_t>(displayedRange.location);
		if (trackingRightBase > static_cast<slim_position_t>((displayedRange.location + displayedRange.length) - 1))
			trackingRightBase = static_cast<slim_position_t>((displayedRange.location + displayedRange.length) - 1);
		
		if (trackingRightBase <= trackingLeftBase + 3)      // minimum selection length is 5; below that, reset to no selection
		{
			if (hasSelection_)
				selectionChanged = true;
			
			hasSelection_ = false;
			
			// Save the selection for restoring across recycles, etc.
			savedHasSelection_ = hasSelection_;
		}
        else if (movedSufficiently_ || (abs(initialMouseX - curPoint.x()) > 2))  // movement threshold before drag-selection begins
		{
			selectionChanged = true;
			hasSelection_ = true;
            movedSufficiently_ = true;
			selectionFirstBase_ = trackingLeftBase;
			selectionLastBase_ = trackingRightBase;
            simpleClickInFocalChromosome_ = false;  // no resetting to overview
			
			// Save the selection for restoring across recycles, etc.
			savedSelectionFirstBase_ = selectionFirstBase_;
			savedSelectionLastBase_ = selectionLastBase_;
			savedHasSelection_ = hasSelection_;
		}
		
		if (selectionChanged)
		{
			update();
            updateDependentView();
		}
	}
}

void QtSLiMChromosomeWidget::mouseMoveEvent(QMouseEvent *p_event)
{
    if (isOverview_ && isTracking_)
		_mouseTrackEvent(p_event);
}

void QtSLiMChromosomeWidget::mouseReleaseEvent(QMouseEvent *p_event)
{
    if (isOverview_ && isTracking_)
	{
        _mouseTrackEvent(p_event);
        
        // prevent a flip to showing chromosome numbers after user tracking
        mouseInsideCounter_++;
        showChromosomeNumbers_ = false;
        
        // if we had a simple click and mouse-up in the focal chromosome, and there
        // was no existing selection, then reset to showing all chromosomes
        if (simpleClickInFocalChromosome_)
        {
            setFocalChromosome(nullptr);
            update();
            updateDependentView();
        }
	}
	
	isTracking_ = false;
}

void QtSLiMChromosomeWidget::contextMenuEvent(QContextMenuEvent * /* p_event */)
{
    // BCH 5/9/2022: I think now that we can have multiple chromosome views it might be best to make
    // people use the action button; a context menu running on a particular view looks view-specific,
    // but the multiple chromosome views share all their configuration state, so that would be odd.
    
    //if (!isOverview_)
    //    controller_->runChromosomeContextMenuAtPoint(p_event->globalPos());
}

void QtSLiMChromosomeWidget::enterEvent(QTSLIM_ENTER_EVENT * /* event */)
{
    if (isOverview_)
    {
        // When the mouse enters, we want to switch to showing chromosome numbers, but we want it to
        // happen with a bit of a delay so it doesn't flip visually when the user is just moving the
        // mouse around.  We want the display change not to happen again if the mouse exits before
        // the delay is up, *even* if it re-enters again within the delay period.  To achieve that,
        // we use a unique identifier for each entry, in the form of a counter, mouseInsideCounter_.
        // We use a one-second delay to give the user time to start dragging a selection if they
        // want to; that would often depend upon the genomic elements, so we don't want to hide them.
        mouseInside_ = true;
        mouseInsideCounter_++;
        
        int thisMouseInsideCounter = mouseInsideCounter_;
        
        QTimer::singleShot(1000, this,
            [this, thisMouseInsideCounter]() {
                if (mouseInside_ && (mouseInsideCounter_ == thisMouseInsideCounter))
                {
                    showChromosomeNumbers_ = true;
                    update();
                }
            });
    }
}

void QtSLiMChromosomeWidget::leaveEvent(QEvent * /* event */)
{
    if (isOverview_)
    {
        mouseInside_ = false;
        mouseInsideCounter_++;
        
        if (showChromosomeNumbers_)
        {
            // When the mouse exists, we want to switch away from showing chromosome numbers, but we
            // again want it to happen with a bit of delay, so that the user can mouse over to the
            // chromosome number they want without having it flip back due to a mouse track that
            // passes outside the overview strip.  So we want the display change not to happen if
            // the mouse enters again within that delay.  We can use the same mechanism as above.
            int thisMouseInsideCounter = mouseInsideCounter_;
            
            QTimer::singleShot(500, this,
                [this, thisMouseInsideCounter]() {
                    if (!mouseInside_ && (mouseInsideCounter_ == thisMouseInsideCounter))
                    {
                        showChromosomeNumbers_ = false;
                        update();
                    }
                });
        }
    }
}


























