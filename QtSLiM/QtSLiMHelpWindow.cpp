#include "QtSLiMHelpWindow.h"
#include "ui_QtSLiMHelpWindow.h"

#include <QLineEdit>
#include <QIcon>
#include <QDebug>
#include <QSettings>
#include <QCloseEvent>

#include "eidos_interpreter.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_sim.h"
#include "chromosome.h"
#include "genome.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "individual.h"
#include "subpopulation.h"

#include <vector>
#include <algorithm>


QtSLiMHelpWindow &QtSLiMHelpWindow::instance(void)
{
    static QtSLiMHelpWindow inst(nullptr);
    
    return inst;
}

QtSLiMHelpWindow::QtSLiMHelpWindow(QWidget *parent) : QDialog(parent), ui(new Ui::QtSLiMHelpWindow)
{
    ui->setupUi(this);
    
    // Configure the search field to look like a search field
    const QIcon searchIcon(":/help/search.png");
    QAction *searchAction = new QAction(searchIcon, "&Search...", this);
    
    ui->searchField->setClearButtonEnabled(true);
    ui->searchField->addAction(searchAction, QLineEdit::LeadingPosition);
    ui->searchField->setPlaceholderText("Search...");
    
    connect(ui->searchField, &QLineEdit::returnPressed, this, &QtSLiMHelpWindow::searchChanged);
    connect(searchAction, &QAction::triggered, this, &QtSLiMHelpWindow::searchChanged);
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMHelpWindow");
    resize(settings.value("size", QSize(550, 400)).toSize());
    move(settings.value("pos", QPoint(25, 45)).toPoint());
    settings.endGroup();
    
    // Add Eidos topics
    addTopicsFromRTFFile("EidosHelpFunctions", "1. Eidos Functions", &EidosInterpreter::BuiltInFunctions(), nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpMethods", "2. Eidos Methods", nullptr, gEidos_UndefinedClassObject->Methods(), nullptr);
    addTopicsFromRTFFile("EidosHelpOperators", "3. Eidos Operators", nullptr, nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpStatements", "4. Eidos Statements", nullptr, nullptr, nullptr);
    addTopicsFromRTFFile("EidosHelpTypes", "5. Eidos Types", nullptr, nullptr, nullptr);
    
    // Check for completeness of the Eidos documentation
    checkDocumentationOfFunctions(&EidosInterpreter::BuiltInFunctions());
    checkDocumentationOfClass(gEidos_UndefinedClassObject);
    checkDocumentationForDuplicatePointers();
    
    // Add SLiM topics
    const std::vector<EidosFunctionSignature_SP> *zg_functions = SLiMSim::ZeroGenerationFunctionSignatures();
    const std::vector<EidosFunctionSignature_SP> *slim_functions = SLiMSim::SLiMFunctionSignatures();
    std::vector<EidosFunctionSignature_SP> all_slim_functions;
    
    all_slim_functions.insert(all_slim_functions.end(), zg_functions->begin(), zg_functions->end());
    all_slim_functions.insert(all_slim_functions.end(), slim_functions->begin(), slim_functions->end());
    
    addTopicsFromRTFFile("SLiMHelpFunctions", "6. SLiM Functions", &all_slim_functions, nullptr, nullptr);
    addTopicsFromRTFFile("SLiMHelpClasses", "7. SLiM Classes", nullptr, slimguiAllMethodSignatures(), slimguiAllPropertySignatures());
    addTopicsFromRTFFile("SLiMHelpCallbacks", "8. SLiM Events and Callbacks", nullptr, nullptr, nullptr);
    
    // Check for completeness of the SLiM documentation
    checkDocumentationOfFunctions(&all_slim_functions);
    
    checkDocumentationOfClass(gSLiM_Chromosome_Class);
    checkDocumentationOfClass(gSLiM_Genome_Class);
    checkDocumentationOfClass(gSLiM_GenomicElement_Class);
    checkDocumentationOfClass(gSLiM_GenomicElementType_Class);
    checkDocumentationOfClass(gSLiM_Individual_Class);
    checkDocumentationOfClass(gSLiM_InteractionType_Class);
    checkDocumentationOfClass(gSLiM_Mutation_Class);
    checkDocumentationOfClass(gSLiM_MutationType_Class);
    checkDocumentationOfClass(gSLiM_SLiMEidosBlock_Class);
    checkDocumentationOfClass(gSLiM_SLiMSim_Class);
    checkDocumentationOfClass(gSLiM_Subpopulation_Class);
    checkDocumentationOfClass(gSLiM_Substitution_Class);
    //checkDocumentationOfClass(gSLiM_SLiMgui_Class);
    
    checkDocumentationForDuplicatePointers();
}

QtSLiMHelpWindow::~QtSLiMHelpWindow()
{
    delete ui;
}

void QtSLiMHelpWindow::searchChanged(void)
{
    QString searchString = ui->searchField->text();
    
    ui->searchField->selectAll();
    
    // search for searchString, open the outline view to the right hits, display the results
}

void QtSLiMHelpWindow::closeEvent(QCloseEvent *event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMHelpWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // use super's default behavior
    QDialog::closeEvent(event);
}

void QtSLiMHelpWindow::addTopicsFromRTFFile(const QString &rtfFile,
                                            const QString &topLevelHeading,
                                            const std::vector<EidosFunctionSignature_SP> *functionList,
                                            const std::vector<const EidosMethodSignature*> *methodList,
                                            const std::vector<const EidosPropertySignature*> *propertyList)
{
    
}

const std::vector<const EidosPropertySignature*> *QtSLiMHelpWindow::slimguiAllPropertySignatures(void)
{
    // This adds the properties belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<const EidosPropertySignature*> *propertySignatures = nullptr;
	
	if (!propertySignatures)
	{
		auto slimProperties =					SLiMSim::AllPropertySignatures();
		//auto propertiesSLiMgui =				gSLiM_SLiMgui_Class->Properties();
		
		propertySignatures = new std::vector<const EidosPropertySignature*>(*slimProperties);
		
		//propertySignatures->insert(propertySignatures->end(), propertiesSLiMgui->begin(), propertiesSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllPropertySignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(propertySignatures->begin(), propertySignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(propertySignatures->begin(), propertySignatures->end());
		propertySignatures->resize(static_cast<size_t>(std::distance(propertySignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(propertySignatures->begin(), propertySignatures->end(), CompareEidosPropertySignatures);
		
		const EidosPropertySignature *previous_sig = nullptr;
		
		for (const EidosPropertySignature *sig : *propertySignatures)
		{
			if (previous_sig && (sig->property_name_.compare(previous_sig->property_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the property signatures are identical.
				if ((sig->property_id_ != previous_sig->property_id_) ||
					(sig->read_only_ != previous_sig->read_only_) ||
					(sig->value_mask_ != previous_sig->value_mask_) ||
					(sig->value_class_ != previous_sig->value_class_))
				std::cout << "Duplicate property name with different signature: " << sig->property_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return propertySignatures;
}

const std::vector<const EidosMethodSignature*> *QtSLiMHelpWindow::slimguiAllMethodSignatures(void)
{
    // This adds the methods belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<const EidosMethodSignature*> *methodSignatures = nullptr;
	
	if (!methodSignatures)
	{
		auto slimMethods =					SLiMSim::AllMethodSignatures();
		//auto methodsSLiMgui =				gSLiM_SLiMgui_Class->Methods();
		
		methodSignatures = new std::vector<const EidosMethodSignature*>(*slimMethods);
		
		//methodSignatures->insert(methodSignatures->end(), methodsSLiMgui->begin(), methodsSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllMethodSignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(methodSignatures->begin(), methodSignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(methodSignatures->begin(), methodSignatures->end());
		methodSignatures->resize(static_cast<size_t>(std::distance(methodSignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(methodSignatures->begin(), methodSignatures->end(), CompareEidosCallSignatures);
		
		const EidosMethodSignature *previous_sig = nullptr;
		
		for (const EidosMethodSignature *sig : *methodSignatures)
		{
			if (previous_sig && (sig->call_name_.compare(previous_sig->call_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
				if ((typeid(*sig) != typeid(*previous_sig)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->call_name_ != previous_sig->call_name_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
				std::cout << "Duplicate method name with a different signature: " << sig->call_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
		
		// log a full list
		//std::cout << "----------------" << std::endl;
		//for (const EidosMethodSignature *sig : *methodSignatures)
		//	std::cout << sig->call_name_ << " (" << sig << ")" << std::endl;
	}
	
	return methodSignatures;
}

void QtSLiMHelpWindow::checkDocumentationOfFunctions(const std::vector<EidosFunctionSignature_SP> *functions)
{
}

void QtSLiMHelpWindow::checkDocumentationOfClass(EidosObjectClass *classObject)
{
}

void QtSLiMHelpWindow::checkDocumentationForDuplicatePointers(void)
{
}




























