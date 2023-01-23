//
//  main.cpp
//  SLiM
//
//  Created by Ben Haller on 12/12/14.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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

/*
 
 This file defines main() and related functions that initiate and run a SLiM simulation.
 
 */


#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctime>
#include <chrono>
#include <sys/stat.h>

#include "community.h"
#include "species.h"
#include "eidos_globals.h"
#include "slim_globals.h"
#include "eidos_test.h"
#include "slim_test.h"
#include "eidos_symbol_table.h"

// Get our Git commit SHA-1, as C string "g_GIT_SHA1"
#include "../cmake/GitSHA1.h"


static void PrintUsageAndDie(bool p_print_header, bool p_print_full_usage)
{
	if (p_print_header)
	{
		SLIM_OUTSTREAM << "SLiM version " << SLIM_VERSION_STRING << ", built " << __DATE__ << " " __TIME__ << "." << std::endl;
		
		if (strcmp(g_GIT_SHA1, "GITDIR-NOTFOUND") == 0)
			SLIM_OUTSTREAM << "Git commit SHA-1: unknown (built from a non-Git source archive)" << std::endl;
		else
			SLIM_OUTSTREAM << "Git commit SHA-1: " << std::string(g_GIT_SHA1) << std::endl;
		
#ifdef DEBUG
		SLIM_OUTSTREAM << "This is a DEBUG build of SLiM." << std::endl;
#else
		SLIM_OUTSTREAM << "This is a RELEASE build of SLiM." << std::endl;
#endif
#ifdef _OPENMP
		SLIM_OUTSTREAM << "This is a PARALLEL (MULTI-THREADED) build of SLiM." << std::endl;
#else
		// For now, I don't want to advertise the existence of parallel builds
		//SLIM_OUTSTREAM << "This is a NON-PARALLEL (SINGLE-THREADED) build of SLiM." << std::endl;
#endif
#if (SLIMPROFILING == 1)
		SLIM_OUTSTREAM << "This is a PROFILING build of SLiM." << std::endl;
#endif
		SLIM_OUTSTREAM << std::endl;
		
		SLIM_OUTSTREAM << "SLiM is a product of the Messer Lab, http://messerlab.org/" << std::endl;
		SLIM_OUTSTREAM << "Copyright 2013-2023 Philipp Messer.  All rights reserved." << std::endl << std::endl;
		SLIM_OUTSTREAM << "By Benjamin C. Haller, http://benhaller.com/, and Philipp Messer." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "SLiM home page: http://messerlab.org/slim/" << std::endl;
		SLIM_OUTSTREAM << "slim-announce mailing list: https://groups.google.com/d/forum/slim-announce" << std::endl;
		SLIM_OUTSTREAM << "slim-discuss mailing list: https://groups.google.com/d/forum/slim-discuss" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;

		SLIM_OUTSTREAM << "SLiM is free software: you can redistribute it and/or modify it under the terms" << std::endl;
		SLIM_OUTSTREAM << "of the GNU General Public License as published by the Free Software Foundation," << std::endl;
		SLIM_OUTSTREAM << "either version 3 of the License, or (at your option) any later version." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;" << std::endl;
		SLIM_OUTSTREAM << "without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR" << std::endl;
		SLIM_OUTSTREAM << "PURPOSE.  See the GNU General Public License for more details." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "You should have received a copy of the GNU General Public License along with" << std::endl;
		SLIM_OUTSTREAM << "SLiM.  If not, see <http://www.gnu.org/licenses/>." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;
	}
	
	SLIM_OUTSTREAM << "usage: slim -v[ersion] | -u[sage] | -h[elp] | -testEidos | -testSLiM |" << std::endl;
	SLIM_OUTSTREAM << "   [-l[ong] [<l>]] [-s[eed] <seed>] [-t[ime]] [-m[em]] [-M[emhist]] [-x]" << std::endl;
	SLIM_OUTSTREAM << "   [-d[efine] <def>] ";
#ifdef _OPENMP
	// The -maxthreads flag is visible only for a parallel build
	SLIM_OUTSTREAM << "[-maxthreads <n>] ";
#endif
#if (SLIMPROFILING == 1)
	// Some flags are visible only for a profile build
	SLIM_OUTSTREAM << "[<profile-flags>] ";
#endif
	SLIM_OUTSTREAM << "[<script file>]" << std::endl;
	
	if (p_print_full_usage)
	{
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "   -v[ersion]         : print SLiM's version information" << std::endl;
		SLIM_OUTSTREAM << "   -u[sage]           : print command-line usage help" << std::endl;
		SLIM_OUTSTREAM << "   -h[elp]            : print full help information" << std::endl;
		SLIM_OUTSTREAM << "   -testEidos | -te   : run built-in self-diagnostic tests of Eidos" << std::endl;
		SLIM_OUTSTREAM << "   -testSLiM | -ts    : run built-in self-diagnostic tests of SLiM" << std::endl;
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "   -l[ong] [<l>]      : long (i.e., verbose) output of level <l> (default 2)" << std::endl;
		SLIM_OUTSTREAM << "   -s[eed] <seed>     : supply an initial random number seed for SLiM" << std::endl;
		SLIM_OUTSTREAM << "   -t[ime]            : print SLiM's total execution time (in user clock time)" << std::endl;
		SLIM_OUTSTREAM << "   -m[em]             : print SLiM's peak memory usage" << std::endl;
		SLIM_OUTSTREAM << "   -M[emhist]         : print a histogram of SLiM's memory usage" << std::endl;
		SLIM_OUTSTREAM << "   -x                 : disable SLiM's runtime safety/consistency checks" << std::endl;
		SLIM_OUTSTREAM << "   -d[efine] <def>    : define an Eidos constant, such as \"mu=1e-7\"" << std::endl;
#ifdef _OPENMP
		// The -maxthreads flag is visible only for a parallel build
		SLIM_OUTSTREAM << "   -maxthreads <n>    : set the maximum number of threads used" << std::endl;
#endif
#if (SLIMPROFILING == 1)
		SLIM_OUTSTREAM << "   " << std::endl;
		SLIM_OUTSTREAM << "   <profile-flags>:" << std::endl;
		
		SLIM_OUTSTREAM << "   -profileStart <n>  : set the first tick to profile" << std::endl;
		SLIM_OUTSTREAM << "   -profileEnd <n>    : set the last tick to profile" << std::endl;
		SLIM_OUTSTREAM << "   -profileOut <path> : set a path for profiling output (default profile.html)" << std::endl;
		SLIM_OUTSTREAM << "   " << std::endl;
#endif
		SLIM_OUTSTREAM << "   <script file>      : the input script file (stdin may be used instead)" << std::endl;
	}
	
	if (p_print_header || p_print_full_usage)
		SLIM_OUTSTREAM << std::endl;
	
	exit(EXIT_SUCCESS);
}

#if SLIM_LEAK_CHECKING
static void clean_up_leak_false_positives(void)
{
	// This does a little cleanup that helps Valgrind to understand that some things have not been leaked.
	// I think perhaps unordered_map keeps values in an unaligned manner that Valgrind doesn't see as pointers.
	MutationRun::DeleteMutationRunFreeList();
	InteractionType::DeleteSparseVectorFreeList();
	FreeSymbolTablePool();
	Eidos_FreeRNG(gEidos_RNG);
}
#endif

static void test_exit(int test_result)
{
#if SLIM_LEAK_CHECKING
	clean_up_leak_false_positives();
	
	// sleep() to give time to assess leaks at the command line
	std::cout << "\nSLEEPING" << std::endl;
	sleep(60);
#endif
	
	exit(test_result);
}

#if (SLIMPROFILING == 1)
//
//	Support for profiling at the command line, and emitting the profile report in HTML
//	This is very parallel to the profile reporting code in SLiMguiLegacy and QtSLiM,
//	but without Cocoa or Qt, and emitting HTML instead of widget kit formats
//

#if DEBUG
#error In order to obtain accurate timing information that is relevant to the actual runtime of a model, profiling requires that you are running a Release build of SLiM.
#endif

static std::string StringForByteCount(int64_t bytes)
{
	char buf[128];		// used for printf-style formatted strings
	
	if (bytes > 512LL * 1024L * 1024L * 1024L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0));
		return std::string(buf).append(" TB");
	}
	else if (bytes > 512L * 1024L * 1024L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0 * 1024.0 * 1024.0));
		return std::string(buf).append(" GB");
	}
	else if (bytes > 512L * 1024L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0 * 1024.0));
		return std::string(buf).append(" MB");
	}
	else if (bytes > 512L)
	{
		snprintf(buf, 128, "%0.2f", bytes / (1024.0));
		return std::string(buf).append(" KB");
	}
	else
	{
		snprintf(buf, 128, "%lld", bytes);
		return std::string(buf).append(" bytes");
	}
}

#define SLIM_YELLOW_FRACTION 0.10
#define SLIM_SATURATION 0.75

static std::string SpanTagForColorFraction(double color_fraction)
{
	double r, g, b;
	char buf[256];
	
	if (color_fraction < SLIM_YELLOW_FRACTION)
	{
		// small fractions fall on a ramp from white (0.0) to yellow (SLIM_YELLOW_FRACTION)
		r = 1.0;
		g = 1.0;
		b = 1.0 - (color_fraction / SLIM_YELLOW_FRACTION) * SLIM_SATURATION;
	}
	else
	{
		// larger fractions ramp from yellow (SLIM_YELLOW_FRACTION) to red (1.0)
		r = 1.0;
		g = (1.0 - (color_fraction - SLIM_YELLOW_FRACTION) / (1.0 - SLIM_YELLOW_FRACTION));
		b = 0.0;
	}
	
	snprintf(buf, 256, "<span style='background-color:rgb(%d,%d,%d);'>", (int)round(r * 255), (int)round(g * 255), (int)round(b * 255));
	return std::string(buf);
}

static void _ColorScriptWithProfileCounts(const EidosASTNode *node, double elapsedTime, int32_t baseIndex, double *color_fractions)
{
	eidos_profile_t count = node->profile_total_;
	
	if (count > 0)
	{
		int32_t start = 0, end = 0;
		
		node->FullUTF8Range(&start, &end);
		
		start -= baseIndex;
		end -= baseIndex;
		
		double color_fraction = Eidos_ElapsedProfileTime(count) / elapsedTime;
		
		for (int32_t pos = start; pos <= end; pos++)
			color_fractions[pos] = color_fraction;
	}
	
	// Then let child nodes color
	for (const EidosASTNode *child : node->children_)
		_ColorScriptWithProfileCounts(child, elapsedTime, baseIndex, color_fractions);
}

static std::string ColorScriptWithProfileCounts(std::string script, const EidosASTNode *node, double elapsedTime, int32_t baseIndex)
{
	const char *script_data = script.data();
	size_t script_len = script.length();
	double *color_fractions = (double *)calloc(script_len, sizeof(double));
	
	// Color our range, and all children's ranges recursively
	_ColorScriptWithProfileCounts(node, elapsedTime, baseIndex, color_fractions);
	
	// Assemble our result string
	std::string result;
	double last_fraction = -1.0;
	
	for (size_t pos = 0; pos < script_len; ++pos)
	{
		// change the color if necessary
		if (color_fractions[pos] != last_fraction)
		{
			if (pos > 0)
				result.append("</span>");
			
			last_fraction = color_fractions[pos];
			result.append(SpanTagForColorFraction(last_fraction));
		}
		
		// emit the character, escaped as needed
		char ch = script_data[pos];
		
		if (ch == '&') result.append("&amp;");
		else if (ch == '<') result.append("&lt;");
		else if (ch == '>') result.append("&gt;");
		else if (ch == '\n') result.append("<BR>\n");
		else if (ch == '\r') ;
		else if (ch == '\t') result.append("&nbsp;&nbsp;&nbsp;");
		else result.append(1, ch);
	}
	
	if (script_len > 0)
		result.append("</span>");
	
	free(color_fractions);
	return result;
}

static std::string ColoredSpanForByteCount(int64_t bytes, double total)
{
	std::string byteString = StringForByteCount(bytes);
	double fraction = bytes / total;
	std::string spanTag = SpanTagForColorFraction(fraction);
	
	spanTag.append(byteString).append("</span>");
	return spanTag;
}

static std::string HTMLEncodeString(const std::string &data)
{
	// This is used to escape characters that have a special meaning in HTML
	std::string buffer;
	buffer.reserve(data.size());
	
	for (size_t pos = 0; pos != data.size(); ++pos)
	{
		switch (data[pos])
		{
			case '&':	buffer.append("&amp;");       			break;
			case '<':	buffer.append("&lt;");        			break;
			case '>':	buffer.append("&gt;");        			break;
			case '\n':	buffer.append("<BR>\n");				break;
			case '\r':											break;
			case '\t':	buffer.append("&nbsp;&nbsp;&nbsp;");	break;
			default:	buffer.append(&data[pos], 1); break;			// note we specify UTF-8 encoding, so this works with everything
		}
	}
	
	return buffer;
}

static std::string HTMLMakeSpacesNonBreaking(const char *data)
{
	// This is used to convert spaces ni &nbsp;, to make the alignment and formatting work properly
	std::string buffer;
	size_t pos = 0;
	
	while (true)
	{
		char ch = data[pos];
		
		if (ch == 0)				break;
		else if (data[pos] == ' ')	buffer.append("&nbsp;");
		else						buffer.append(&data[pos], 1);
		
		pos++;
	}
	
	return buffer;
}

static void WriteProfileResults(std::string profile_output_path, std::string model_name, Community *community)
{
	std::ofstream fout(profile_output_path);
	char buf[256];		// used for printf-style formatted strings
	
	//
	//	HTML header
	//
	
	fout << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">" << "\n";
	fout << "<html>" << "\n";
	fout << "<head>" << "\n";
	fout << "<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>" << "\n";
	fout << "<meta http-equiv='Content-Style-Type' content='text/css'>" << "\n";
	fout << "<title>Profile Report</title>" << "\n";
	fout << "<meta name='Generator' content='SLiM'>" << "\n";
	fout << "<style type='text/css'>" << "\n";
	fout << "    body {font-family: Optima,sans-serif; font-size: 14;}" << "\n";
	fout << "    h2 {margin-bottom: 0px;}" << "\n";
	fout << "    h3 {margin-bottom: 0px;}" << "\n";
	fout << "    p {margin-top: 5px;}" << "\n";
	fout << "    tt {font-family: Menlo,monospace; font-size: 13;}" << "\n";
	fout << "</style>" << "\n";
	fout << "</head>" << "\n\n";
	fout << "<body>" << "\n";
	
	
	//
	//	Profile header
	//
	
	double elapsedWallClockTime = (std::chrono::duration_cast<std::chrono::microseconds>(community->profile_end_clock - community->profile_start_clock).count()) / (double)1000000L;
	double elapsedCPUTimeInSLiM = community->profile_elapsed_CPU_clock / (double)CLOCKS_PER_SEC;
	double elapsedWallClockTimeInSLiM = Eidos_ElapsedProfileTime(community->profile_elapsed_wall_clock);
	slim_tick_t elapsedSLiMTicks = community->profile_end_tick - community->profile_start_tick;
	
	fout << "<h2>Profile Report" << "</h2>\n";
	fout << "<p>Model: " << model_name << "</p>\n\n";
	
	std::string start_date_string(ctime(&community->profile_start_date));
	std::string end_date_string(ctime(&community->profile_end_date));
	
	start_date_string.pop_back();		// remove the trailing newline that ctime() annoyingly adds
	end_date_string.pop_back();
	
	fout << "<p>Run start: " << start_date_string << "<BR>\n";
	fout << "Run end: " << end_date_string << "</p>\n\n";
	
	
	snprintf(buf, 256, "%0.2f s", elapsedWallClockTime);
	fout << "<p>Elapsed wall clock time: " << buf << "<BR>\n";
	
	snprintf(buf, 256, "%0.2f s", elapsedCPUTimeInSLiM);
	fout << "Elapsed wall clock time inside SLiM core (corrected): " << buf << "<BR>\n";
	
	snprintf(buf, 256, "%0.2f", elapsedWallClockTimeInSLiM);
	fout << "Elapsed CPU time inside SLiM core (uncorrected): " << buf << " s" << "<BR>\n";
	
	fout << "Elapsed ticks: " << elapsedSLiMTicks << ((community->profile_start_tick == 0) ? " (including initialize)" : "") << "</p>\n\n";
	
	
	snprintf(buf, 256, "%0.2f ticks (%0.4g s)", gEidos_ProfileOverheadTicks, gEidos_ProfileOverheadSeconds);
	fout << "<p>Profile block external overhead: " << buf << "<BR>\n";
	
	snprintf(buf, 256, "%0.2f ticks (%0.4g s)", gEidos_ProfileLagTicks, gEidos_ProfileLagSeconds);
	fout << "Profile block internal lag: " << buf << "</p>\n\n";
	
	size_t total_usage = community->profile_total_memory_usage_Community.totalMemoryUsage + community->profile_total_memory_usage_AllSpecies.totalMemoryUsage;
	size_t average_usage = total_usage / community->total_memory_tallies_;
	size_t last_usage = community->profile_last_memory_usage_Community.totalMemoryUsage + community->profile_last_memory_usage_AllSpecies.totalMemoryUsage;
	
	fout << "<p>Average tick SLiM memory use: " << StringForByteCount(average_usage) << "<BR>\n";
	fout << "Final tick SLiM memory use: " << StringForByteCount(last_usage) << "</p>\n\n";
	
	
	//
	//	Cycle stage breakdown
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		bool isWF = (community->ModelType() == SLiMModelType::kModelTypeWF);
		double elapsedStage0Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[6]);
		double elapsedStage7Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[7]);
		double elapsedStage8Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[8]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage7 = (elapsedStage7Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage8 = (elapsedStage8Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage0Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage1Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage2Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage3Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage4Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage5Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage6Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage7Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage8Time));
		
		fout << "<h3>Cycle stage breakdown" << "</h3>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage0Time, percentStage0);
		fout << "<p>" << HTMLMakeSpacesNonBreaking(buf) << " : initialize() callback execution" << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage1Time, percentStage1);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 0 - first() event execution" : " : stage 0 - first() event execution") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage2Time, percentStage2);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 1 - early() event execution" : " : stage 1 - offspring generation") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage3Time, percentStage3);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 2 - offspring generation" : " : stage 2 - early() event execution") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage4Time, percentStage4);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 3 - bookkeeping (fixed mutation removal, etc.)" : " : stage 3 - fitness calculation") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage5Time, percentStage5);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 4 - generation swap" : " : stage 4 - viability/survival selection") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage6Time, percentStage6);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 5 - late() event execution" : " : stage 5 - bookkeeping (fixed mutation removal, etc.)") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage7Time, percentStage7);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 6 - fitness calculation" : " : stage 6 - late() event execution") << "<BR>\n";
		
		snprintf(buf, 256, "<tt>%*.2f s (%5.2f%%)</tt>", fw, elapsedStage8Time, percentStage8);
		fout << HTMLMakeSpacesNonBreaking(buf) << (isWF ? " : stage 7 - tree sequence auto-simplification" : " : stage 7 - tree sequence auto-simplification") << "</p>\n\n";
	}
	
	
	//
	//	Callback type breakdown
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		double elapsedTime_first = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventFirst]);
		double elapsedTime_early = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventEarly]);
		double elapsedTime_late = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventLate]);
		double elapsedTime_initialize = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosInitializeCallback]);
		double elapsedTime_mutationEffect = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMutationEffectCallback]);
		double elapsedTime_fitnessEffect = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosFitnessEffectCallback]);
		double elapsedTime_interaction = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosInteractionCallback]);
		double elapsedTime_matechoice = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMateChoiceCallback]);
		double elapsedTime_modifychild = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosModifyChildCallback]);
		double elapsedTime_recombination = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosRecombinationCallback]);
		double elapsedTime_mutation = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMutationCallback]);
		double elapsedTime_reproduction = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosReproductionCallback]);
		double elapsedTime_survival = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosSurvivalCallback]);
		double percent_first = (elapsedTime_first / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_early = (elapsedTime_early / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_late = (elapsedTime_late / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_initialize = (elapsedTime_initialize / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_fitness = (elapsedTime_mutationEffect / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_fitnessglobal = (elapsedTime_fitnessEffect / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_interaction = (elapsedTime_interaction / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_matechoice = (elapsedTime_matechoice / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_modifychild = (elapsedTime_modifychild / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_recombination = (elapsedTime_recombination / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_mutation = (elapsedTime_mutation / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_reproduction = (elapsedTime_reproduction / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_survival = (elapsedTime_survival / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4, fw2 = 4;
		
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_first));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_early));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_late));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_initialize));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_mutationEffect));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_fitnessEffect));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_interaction));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_matechoice));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_modifychild));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_recombination));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_mutation));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_reproduction));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_survival));
		
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_first));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_early));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_late));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_initialize));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_fitness));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_fitnessglobal));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_interaction));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_matechoice));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_modifychild));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_recombination));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_mutation));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_reproduction));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_survival));
		
		fout << "<h3>Callback type breakdown" << "</h3>\n";
		
		// Note these are out of numeric order, but in cycle stage order
		if (community->ModelType() == SLiMModelType::kModelTypeWF)
		{
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_initialize, fw2, percent_initialize);
			fout << "<p>" << HTMLMakeSpacesNonBreaking(buf) << " : initialize() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_first, fw2, percent_first);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : first() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_early, fw2, percent_early);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : early() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_matechoice, fw2, percent_matechoice);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mateChoice() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_recombination, fw2, percent_recombination);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : recombination() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutation, fw2, percent_mutation);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutation() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_modifychild, fw2, percent_modifychild);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : modifyChild() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_late, fw2, percent_late);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : late() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutationEffect, fw2, percent_fitness);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutationEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_fitnessEffect, fw2, percent_fitnessglobal);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : fitnessEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_interaction, fw2, percent_interaction);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : interaction() callbacks" << "</p>\n\n";
			
		}
		else
		{
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_initialize, fw2, percent_initialize);
			fout << "<p>" << HTMLMakeSpacesNonBreaking(buf) << " : initialize() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_first, fw2, percent_first);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : first() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_reproduction, fw2, percent_reproduction);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : reproduction() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_recombination, fw2, percent_recombination);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : recombination() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutation, fw2, percent_mutation);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutation() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_modifychild, fw2, percent_modifychild);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : modifyChild() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_early, fw2, percent_early);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : early() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_mutationEffect, fw2, percent_fitness);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : mutationEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_fitnessEffect, fw2, percent_fitnessglobal);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : fitnessEffect() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_survival, fw2, percent_survival);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : survival() callbacks" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_late, fw2, percent_late);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : late() events" << "<BR>\n";
			
			snprintf(buf, 256, "<tt>%*.2f s (%*.2f%%)</tt>", fw, elapsedTime_interaction, fw2, percent_interaction);
			fout << HTMLMakeSpacesNonBreaking(buf) << " : interaction() callbacks" << "</p>\n\n";
		}
	}
	
	
	//
	//	Script block profiles
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		{
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			
			// Convert the profile counts in all script blocks into self counts (excluding the counts of nodes below them)
			for (SLiMEidosBlock *script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					script_block->root_node_->ConvertProfileTotalsToSelfCounts();
		}
		
		{
			fout << "<h3>Script block profiles (as a fraction of corrected wall clock time)" << "</h3>\n";
			
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			bool hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>";
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
		
		{
			fout << "<h3>Script block profiles (as a fraction of within-block wall clock time)" << "</h3>\n";
			
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			bool hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>";
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, total_block_time, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
	}
	
	
	//
	//	User-defined functions (if any)
	//
	
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		EidosFunctionMap &function_map = community->FunctionMap();
		std::vector<const EidosFunctionSignature *> userDefinedFunctions;
		
		for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
		{
			const EidosFunctionSignature *signature = functionPairIter->second.get();
			
			if (signature->body_script_ && signature->user_defined_)
			{
				signature->body_script_->AST()->ConvertProfileTotalsToSelfCounts();
				userDefinedFunctions.emplace_back(signature);
			}
		}
		
		if (userDefinedFunctions.size())
		{
			fout << "<h3>User-defined functions (as a fraction of corrected wall clock time)" << "</h3>\n";
			
			bool hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					const std::string &&signature_string = signature->SignatureString();
					
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>" << HTMLEncodeString(signature_string) << "<BR>";
					
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
		
		if (userDefinedFunctions.size())
		{
			fout << "<h3>User-defined functions (as a fraction of within-block wall clock time)" << "</h3>\n";
			
			bool hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					const std::string &&signature_string = signature->SignatureString();
					
					snprintf(buf, 256, "<p style='margin-bottom: 5px;'><tt>%0.2f s (%0.2f%%):</tt></p>", total_block_time, percent_block_time);
					fout << buf << "\n" << "<p style='margin-top: 5px;'><tt>" << HTMLEncodeString(signature_string) << "<BR>";
					
					fout << ColorScriptWithProfileCounts(profile_root->token_->token_string_, profile_root, total_block_time, profile_root->token_->token_start_) << "</tt></p>\n\n";
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
				fout << "<p><i>(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)" << "</i></p>\n\n";
		}
	}
	
	
#if SLIM_USE_NONNEUTRAL_CACHES
	//
	//	MutationRun metrics, presented per Species
	//
	const std::vector<Species *> &all_species = community->AllSpecies();
	
	for (Species *focal_species : all_species)
	{
		fout << "<h3>MutationRun usage";
		if (all_species.size() > 1)
			fout << " (" << HTMLEncodeString(focal_species->avatar_) << " " << HTMLEncodeString(focal_species->name_) << ")";
		fout << "</h3>\n";
		
		if (!focal_species->HasGenetics())
		{
			fout << "<p><i>(omitted for no-genetics species)" << "</i></p>\n";
			continue;
		}
		
		{
			int64_t power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
			int64_t power_tallies_total = (int)focal_species->profile_mutcount_history_.size();
			
			for (int power = 0; power < 20; ++power)
				power_tallies[power] = 0;
			
			for (int32_t count : focal_species->profile_mutcount_history_)
			{
				int power = (int)round(log2(count));
				
				power_tallies[power]++;
			}
			
			fout << "<p>";
			bool first_line = true;
			
			for (int power = 0; power < 20; ++power)
			{
				if (power_tallies[power] > 0)
				{
					if (!first_line)
						fout << "<BR>\n";
					snprintf(buf, 256, "%6.2f%%", (power_tallies[power] / (double)power_tallies_total) * 100.0);
					fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of ticks : " << ((int)(round(pow(2.0, power)))) << " mutation runs per genome";
					first_line = false;
				}
			}
			
			fout << "</p>\n";
		}
		
		{
			int64_t regime_tallies[3];
			int64_t regime_tallies_total = (int)focal_species->profile_nonneutral_regime_history_.size();
			
			for (int regime = 0; regime < 3; ++regime)
				regime_tallies[regime] = 0;
			
			for (int32_t regime : focal_species->profile_nonneutral_regime_history_)
				if ((regime >= 1) && (regime <= 3))
					regime_tallies[regime - 1]++;
				else
					regime_tallies_total--;
			
			fout << "<p>";
			bool first_line = true;
			
			for (int regime = 0; regime < 3; ++regime)
			{
				if (!first_line)
					fout << "<BR>\n";
				snprintf(buf, 256, "%6.2f%%", (regime_tallies[regime] / (double)regime_tallies_total) * 100.0);
				fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of ticks : regime " << (regime + 1) << " (" << (regime == 0 ? "no mutationEffect() callbacks" : (regime == 1 ? "constant neutral mutationEffect() callbacks only" : "unpredictable mutationEffect() callbacks present")) << ")";
				first_line = false;
			}
			
			fout << "</p>\n";
		}
		
		fout << "<p><tt>" << focal_species->profile_mutation_total_usage_ << "</tt> mutations referenced, summed across all ticks<BR>\n";
		fout << "<tt>" << focal_species->profile_nonneutral_mutation_total_ << "</tt> mutations considered potentially nonneutral<BR>\n";
		snprintf(buf, 256, "%0.2f%%", ((focal_species->profile_mutation_total_usage_ - focal_species->profile_nonneutral_mutation_total_) / (double)focal_species->profile_mutation_total_usage_) * 100.0);
		fout << "<tt>" << buf << "</tt> of mutations excluded from fitness calculations<BR>\n";
		fout << "<tt>" << focal_species->profile_max_mutation_index_ << "</tt> maximum simultaneous mutations</p>\n";
		
		fout << "<p><tt>" << focal_species->profile_mutrun_total_usage_ << "</tt> mutation runs referenced, summed across all ticks<BR>\n";
		fout << "<tt>" << focal_species->profile_unique_mutrun_total_ << "</tt> unique mutation runs maintained among those<BR>\n";
		snprintf(buf, 256, "%6.2f%%", (focal_species->profile_mutrun_nonneutral_recache_total_ / (double)focal_species->profile_unique_mutrun_total_) * 100.0);
		fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of mutation run nonneutral caches rebuilt per tick<BR>\n";
		snprintf(buf, 256, "%6.2f%%", ((focal_species->profile_mutrun_total_usage_ - focal_species->profile_unique_mutrun_total_) / (double)focal_species->profile_mutrun_total_usage_) * 100.0);
		fout << "<tt>" << HTMLMakeSpacesNonBreaking(buf) << "</tt> of mutation runs shared among genomes</p>\n\n";
	}
#endif
	
	//
	//	Memory usage metrics
	//
	
	{
		fout << "<h3>SLiM memory usage (average / final tick) " << "</h3>\n";
		
		SLiMMemoryUsage_Community &mem_tot_C = community->profile_total_memory_usage_Community;
		SLiMMemoryUsage_Species &mem_tot_S = community->profile_total_memory_usage_AllSpecies;
		SLiMMemoryUsage_Community &mem_last_C = community->profile_last_memory_usage_Community;
		SLiMMemoryUsage_Species &mem_last_S = community->profile_last_memory_usage_AllSpecies;
		int64_t div = community->total_memory_tallies_;
		double ddiv = community->total_memory_tallies_;
		double average_total = (mem_tot_C.totalMemoryUsage + mem_tot_S.totalMemoryUsage) / ddiv;
		double final_total = mem_last_C.totalMemoryUsage + mem_last_S.totalMemoryUsage;
		
		// Chromosome
		snprintf(buf, 256, "%0.2f", mem_tot_S.chromosomeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.chromosomeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeObjects, final_total) << "</tt> : Chromosome objects (" << buf << " / " << mem_last_S.chromosomeObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.chromosomeMutationRateMaps / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeMutationRateMaps, final_total) << "</tt> : mutation rate maps<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.chromosomeRecombinationRateMaps / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeRecombinationRateMaps, final_total) << "</tt> : recombination rate maps<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.chromosomeAncestralSequence / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.chromosomeAncestralSequence, final_total) << "</tt> : ancestral nucleotides</p>\n\n";
		
		// Community
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_C.communityObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.communityObjects, final_total) << "</tt> : Community object</p>\n\n";
		
		// Genome
		snprintf(buf, 256, "%0.2f", mem_tot_S.genomeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.genomeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeObjects, final_total) << "</tt> : Genome objects (" << buf << " / " << mem_last_S.genomeObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.genomeExternalBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeExternalBuffers, final_total) << "</tt> : external MutationRun* buffers<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.genomeUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeUnusedPoolSpace, final_total) << "</tt> : unused pool space<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.genomeUnusedPoolBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomeUnusedPoolBuffers, final_total) << "</tt> : unused pool buffers</p>\n\n";
		
		// GenomicElement
		snprintf(buf, 256, "%0.2f", mem_tot_S.genomicElementObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.genomicElementObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomicElementObjects, final_total) << "</tt> : GenomicElement objects (" << buf << " / " << mem_last_S.genomicElementObjects_count << ")</p>\n\n";
		
		// GenomicElementType
		snprintf(buf, 256, "%0.2f", mem_tot_S.genomicElementTypeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.genomicElementTypeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.genomicElementTypeObjects, final_total) << "</tt> : GenomicElementType objects (" << buf << " / " << mem_last_S.genomicElementTypeObjects_count << ")</p>\n\n";
		
		// Individual
		snprintf(buf, 256, "%0.2f", mem_tot_S.individualObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.individualObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.individualObjects, final_total) << "</tt> : Individual objects (" << buf << " / " << mem_last_S.individualObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.individualUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.individualUnusedPoolSpace, final_total) << "</tt> : unused pool space</p>\n\n";
		
		// InteractionType
		snprintf(buf, 256, "%0.2f", mem_tot_C.interactionTypeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_C.interactionTypeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypeObjects, final_total) << "</tt> : InteractionType objects (" << buf << " / " << mem_last_C.interactionTypeObjects_count << ")";
		
		if (mem_tot_C.interactionTypeObjects_count || mem_last_C.interactionTypeObjects_count)
		{
			fout << "<BR>\n";	// finish previous line with <BR>
			
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.interactionTypeKDTrees / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypeKDTrees, final_total) << "</tt> : k-d trees<BR>\n";
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.interactionTypePositionCaches / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypePositionCaches, final_total) << "</tt> : position caches<BR>\n";
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.interactionTypeSparseVectorPool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.interactionTypeSparseVectorPool, final_total) << "</tt> : sparse arrays";
		}
		
		fout << "</p>\n\n";
		
		// Mutation
		snprintf(buf, 256, "%0.2f", mem_tot_S.mutationObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.mutationObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationObjects, final_total) << "</tt> : Mutation objects (" << buf << " / " << mem_last_S.mutationObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.mutationRefcountBuffer / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.mutationRefcountBuffer, final_total) << "</tt> : refcount buffer<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.mutationUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.mutationUnusedPoolSpace, final_total) << "</tt> : unused pool space</p>\n\n";
		
		// MutationRun
		snprintf(buf, 256, "%0.2f", mem_tot_S.mutationRunObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.mutationRunObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunObjects, final_total) << "</tt> : MutationRun objects (" << buf << " / " << mem_last_S.mutationRunObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.mutationRunExternalBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunExternalBuffers, final_total) << "</tt> : external MutationIndex buffers<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.mutationRunNonneutralCaches / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationRunNonneutralCaches, final_total) << "</tt> : nonneutral mutation caches<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.mutationRunUnusedPoolSpace / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.mutationRunUnusedPoolSpace, final_total) << "</tt> : unused pool space<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.mutationRunUnusedPoolBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.mutationRunUnusedPoolBuffers, final_total) << "</tt> : unused pool buffers</p>\n\n";
		
		// MutationType
		snprintf(buf, 256, "%0.2f", mem_tot_S.mutationTypeObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.mutationTypeObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.mutationTypeObjects, final_total) << "</tt> : MutationType objects (" << buf << " / " << mem_last_S.mutationTypeObjects_count << ")</p>\n\n";
		
		// Species
		snprintf(buf, 256, "%0.2f", mem_tot_S.speciesObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.speciesObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.speciesObjects, final_total) << "</tt> : Species objects (" << buf << " / " << mem_last_S.speciesObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.speciesTreeSeqTables / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.speciesTreeSeqTables, final_total) << "</tt> : tree-sequence tables</p>\n\n";
		
		// Subpopulation
		snprintf(buf, 256, "%0.2f", mem_tot_S.subpopulationObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.subpopulationObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationObjects, final_total) << "</tt> : Subpopulation objects (" << buf << " / " << mem_last_S.subpopulationObjects_count << ")<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationFitnessCaches / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationFitnessCaches, final_total) << "</tt> : fitness caches<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationParentTables / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationParentTables, final_total) << "</tt> : parent tables<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationSpatialMaps / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationSpatialMaps, final_total) << "</tt> : spatial maps";
		
		if (mem_tot_S.subpopulationSpatialMapsDisplay || mem_last_S.subpopulationSpatialMapsDisplay)
		{
			fout << "<BR>\n";	// finish previous line with <BR>
			
			fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_S.subpopulationSpatialMapsDisplay / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.subpopulationSpatialMapsDisplay, final_total) << "</tt> : spatial map display (SLiMgui only)";
		}
		
		fout << "</p>\n\n";
		
		// Substitution
		snprintf(buf, 256, "%0.2f", mem_tot_S.substitutionObjects_count / ddiv);
		fout << "<p><tt>" << ColoredSpanForByteCount(mem_tot_S.substitutionObjects / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_S.substitutionObjects, final_total) << "</tt> : Substitution objects (" << buf << " / " << mem_last_S.substitutionObjects_count << ")</p>\n\n";
		
		// Eidos
		fout << "<p>Eidos:<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.eidosASTNodePool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.eidosASTNodePool, final_total) << "</tt> : EidosASTNode pool<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.eidosSymbolTablePool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.eidosSymbolTablePool, final_total) << "</tt> : EidosSymbolTable pool<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.eidosValuePool / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.eidosValuePool, final_total) << "</tt> : EidosValue pool<BR>\n";
		fout << "<tt>&nbsp;&nbsp;&nbsp;" << ColoredSpanForByteCount(mem_tot_C.fileBuffers / div, average_total) << "</tt> / <tt>" << ColoredSpanForByteCount(mem_last_C.fileBuffers, final_total) << "</tt> : File buffers</p>\n";
	}
	
	
	//
	//	HTML footer
	//
	
	fout << "</body>" << "\n";
	fout << "</html>" << std::endl;
}
#endif

int main(int argc, char *argv[])
{
	// parse command-line arguments
	unsigned long int override_seed = 0;					// this is the type used for seeds in the GSL
	unsigned long int *override_seed_ptr = nullptr;			// by default, a seed is generated or supplied in the input file
	const char *input_file = nullptr;
	bool keep_time = false, keep_mem = false, keep_mem_hist = false, skip_checks = false, tree_seq_checks = false, tree_seq_force = false;
	std::vector<std::string> defined_constants;
	
#ifdef _OPENMP
	long max_thread_count = omp_get_max_threads();
	bool changed_max_thread_count = false;
#endif
	
#if (SLIMPROFILING == 1)
	slim_tick_t profile_start_tick = 0;
	slim_tick_t profile_end_tick = INT32_MAX;
	std::string profile_output_path = "slim_profile.html";
#endif
	
	// Test the thread-safety check; enable this #if to confirm that this macro is working
	// Note the macro only does its runtime check for a DEBUG build with _OPENMP defined!
#if 0
#pragma omp parallel
	{
		THREAD_SAFETY_CHECK("TEST");
	}
#endif
	
	// command-line SLiM generally terminates rather than throwing
	gEidosTerminateThrows = false;
	
	// "slim" with no arguments prints usage, *unless* stdin is not a tty, in which case we're running the stdin script
	if ((argc == 1) && isatty(fileno(stdin)))
		PrintUsageAndDie(true, false);
	
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char *arg = argv[arg_index];
		
		// -long or -l [<l>]: switches to long (i.e. verbose) output, with an optional integer level specifier
		if (strcmp(arg, "--long") == 0 || strcmp(arg, "-long") == 0 || strcmp(arg, "-l") == 0)
		{
			if (arg_index + 1 == argc)
			{
				// -l[ong] is the last command-line argument, so there is no level; default to 2
				SLiM_verbosity_level = 2;
			}
			else
			{
				// there is another argument following; if it is an integer, we eat it
				const char *s = argv[arg_index + 1];
				bool is_digits = true;
				
				while (*s) {
					if (isdigit(*s++) == 0)
					{
						is_digits = false;
						break;
					}
				}
				
				if (!is_digits)
				{
					// the argument contains non-digit characters, so assume it is not intended for us
					SLiM_verbosity_level = 2;
				}
				else
				{
					errno = 0;
					long verbosity = strtol(argv[arg_index + 1], NULL, 10);
					
					if (errno)
					{
						// the argument did not parse as an integer, so assume it is not intended for us
						SLiM_verbosity_level = 2;
					}
					else
					{
						// the argument parsed as an integer, so it is used to set the verbosity level
						if ((verbosity < 0) || (verbosity > 2))
						{
							SLIM_ERRSTREAM << "Verbosity level supplied to -l[ong] must be 0, 1, or 2." << std::endl;
							exit(EXIT_FAILURE);
						}
						
						SLiM_verbosity_level = verbosity;
						arg_index++;
					}
				}
			}
			
			// SLIM_OUTSTREAM << "// ********** Verbosity level set to " << SLiM_verbosity_level << std::endl << std::endl;
			
			continue;
		}
		
		// -seed <x> or -s <x>: override the default seed with the supplied seed value
		if (strcmp(arg, "--seed") == 0 || strcmp(arg, "-seed") == 0 || strcmp(arg, "-s") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			override_seed = strtol(argv[arg_index], NULL, 10);
			override_seed_ptr = &override_seed;
			
			continue;
		}
		
		// -time or -t: take a time measurement and output it at the end of execution
		if (strcmp(arg, "--time") == 0 || strcmp(arg, "-time") == 0 || strcmp(arg, "-t") == 0)
		{
			keep_time = true;
			
			continue;
		}
		
		// -mem or -m: take a peak memory usage measurement and output it at the end of execution
		if (strcmp(arg, "--mem") == 0 || strcmp(arg, "-mem") == 0 || strcmp(arg, "-m") == 0)
		{
			keep_mem = true;
			
			continue;
		}
		
		// -mem or -m: take a peak memory usage measurement and output it at the end of execution
		if (strcmp(arg, "--Memhist") == 0 || strcmp(arg, "-Memhist") == 0 || strcmp(arg, "-M") == 0)
		{
			keep_mem = true;		// implied by this
			keep_mem_hist = true;
			
			continue;
		}
		
		// -x: skip runtime checks for greater speed, or to avoid them if they are causing problems
		if (strcmp(arg, "-x") == 0)
		{
			skip_checks = true;
			eidos_do_memory_checks = false;
			
			continue;
		}
		
		// -version or -v: print version information
		if (strcmp(arg, "--version") == 0 || strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0)
		{
			SLIM_OUTSTREAM << "SLiM version " << SLIM_VERSION_STRING << ", built " << __DATE__ << " " __TIME__ << std::endl;
			
			if (strcmp(g_GIT_SHA1, "GITDIR-NOTFOUND") == 0)
				SLIM_OUTSTREAM << "Git commit SHA-1: unknown (built from a non-Git source archive)" << std::endl;
			else
				SLIM_OUTSTREAM << "Git commit SHA-1: " << std::string(g_GIT_SHA1) << std::endl;
			
			exit(EXIT_SUCCESS);
		}
		
		// -testEidos or -te: run Eidos tests and quit
		if (strcmp(arg, "--testEidos") == 0 || strcmp(arg, "-testEidos") == 0 || strcmp(arg, "-te") == 0)
		{
#ifdef _OPENMP
			Eidos_WarmUpOpenMP(&SLIM_ERRSTREAM, changed_max_thread_count, (int)max_thread_count, true);
#endif
			Eidos_WarmUp();
			
			gEidosTerminateThrows = true;
			
			int test_result = RunEidosTests();
			
			Eidos_FlushFiles();
			test_exit(test_result);
		}
		
		// -testSLiM or -ts: run SLiM tests and quit
		if (strcmp(arg, "--testSLiM") == 0 || strcmp(arg, "-testSLiM") == 0 || strcmp(arg, "-ts") == 0)
		{
#ifdef _OPENMP
			Eidos_WarmUpOpenMP(&SLIM_ERRSTREAM, changed_max_thread_count, (int)max_thread_count, true);
#endif
			Eidos_WarmUp();
			SLiM_WarmUp();
			
			gEidosTerminateThrows = true;
			
			int test_result = RunSLiMTests();
			
			Eidos_FlushFiles();
			test_exit(test_result);
		}
		
		// -usage or -u: print usage information
		if (strcmp(arg, "--usage") == 0 || strcmp(arg, "-usage") == 0 || strcmp(arg, "-u") == 0 || strcmp(arg, "-?") == 0)
			PrintUsageAndDie(false, true);
		
		// -usage or -u: print full help information
		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-help") == 0 || strcmp(arg, "-h") == 0)
			PrintUsageAndDie(true, true);
		
		// -define or -d: define Eidos constants
		if (strcmp(arg, "--define") == 0 || strcmp(arg, "-define") == 0 || strcmp(arg, "-d") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			defined_constants.emplace_back(argv[arg_index]);
			
			continue;
		}
		
		// -maxthreads <x>: set the maximum number of OpenMP threads that will be used
		if (strcmp(arg, "-maxthreads") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			long count = strtol(argv[arg_index], NULL, 10);
			
#ifdef _OPENMP
			max_thread_count = count;
			changed_max_thread_count = true;
			
			if ((max_thread_count < 1) || (max_thread_count > 1024))
			{
				SLIM_OUTSTREAM << "The -maxthreads command-line option enforces a range of [1, 1024]." << std::endl;
				exit(EXIT_FAILURE);
			}
			
			continue;
#else
			if (count != 1)
			{
				SLIM_OUTSTREAM << "The -maxthreads command-line option only allows a value of 1 when not running a PARALLEL build." << std::endl;
				exit(EXIT_FAILURE);
			}
#endif
		}
		
#if (SLIMPROFILING == 1)
		if (strcmp(arg, "-profileStart") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			long tick = strtol(argv[arg_index], NULL, 10);
			
			if ((tick < 0) || (tick > INT32_MAX))
			{
				SLIM_OUTSTREAM << "The -profileStart command-line option enforces a range of [0, 2000000000]." << std::endl;
				exit(EXIT_FAILURE);
			}
			
			profile_start_tick = (slim_tick_t)tick;
			continue;
		}
		
		if (strcmp(arg, "-profileEnd") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			long tick = strtol(argv[arg_index], NULL, 10);
			
			if ((tick < 0) || (tick > INT32_MAX))
			{
				SLIM_OUTSTREAM << "The -profileEnd command-line option enforces a range of [0, 2000000000]." << std::endl;
				exit(EXIT_FAILURE);
			}
			
			profile_end_tick = (slim_tick_t)tick;
			continue;
		}
		
		if (strcmp(arg, "-profileOut") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			std::string path = std::string(argv[arg_index]);
			
			if (path.length() == 0)
			{
				SLIM_OUTSTREAM << "The -profileOut command-line option requires a non-zero-length filesystem path." << std::endl;
				exit(EXIT_FAILURE);
			}
			
			profile_output_path = path;
			continue;
		}
#endif
		
        // -TSXC is an undocumented command-line flag that turns on tree-sequence recording and runtime crosschecks
        if (strcmp(arg, "-TSXC") == 0)
        {
            tree_seq_checks = true;
            continue;
        }
        
        // -TSF is an undocumented command-line flag that turns on tree-sequence recording without runtime crosschecks
        if (strcmp(arg, "-TSF") == 0)
        {
            tree_seq_force = true;
            continue;
        }
        
		// this is the fall-through, which should be the input file, and should be the last argument given
		if ((arg_index + 1 != argc) || strncmp(arg, "-", 1) == 0)
		{
			SLIM_ERRSTREAM << "Unrecognized command-line argument: " << arg << std::endl << std::endl;
			
			PrintUsageAndDie(false, true);
		}
		
		input_file = argv[arg_index];
	}
	
	// check that we got what we need; if no file was supplied, then stdin must not be a tty (i.e., must be a pipe, etc.)
	if (!input_file && isatty(fileno(stdin)))
		PrintUsageAndDie(false, true);
	
	// announce if we are running a debug build, are skipping runtime checks, etc.
#if DEBUG
	if (SLiM_verbosity_level >= 1)
		SLIM_ERRSTREAM << "// ********** DEBUG defined - you are not using a release build of SLiM" << std::endl << std::endl;
#endif
	
#ifdef _OPENMP
	Eidos_WarmUpOpenMP((SLiM_verbosity_level >= 1) ? &SLIM_ERRSTREAM : nullptr, changed_max_thread_count, (int)max_thread_count, true);
#endif
	
	if (SLiM_verbosity_level >= 2)
		SLIM_ERRSTREAM << "// ********** The -l[ong] command-line option has enabled verbose output (level " << SLiM_verbosity_level << ")" << std::endl << std::endl;
	
	if (skip_checks && (SLiM_verbosity_level >= 1))
		SLIM_ERRSTREAM << "// ********** The -x command-line option has disabled some runtime checks" << std::endl << std::endl;
	
	// emit defined constants in verbose mode
	if (defined_constants.size() && (SLiM_verbosity_level >= 2))
	{
		for (std::string &constant : defined_constants)
			std::cout << "-d[efine]: " << constant << std::endl;
		std::cout << std::endl;
	}
	
	// keep time (we do this whether or not the -time flag was passed)
	std::clock_t begin_cpu = std::clock();
	std::chrono::steady_clock::time_point begin_wall = std::chrono::steady_clock::now();
	
	// keep memory usage information, if asked to
	size_t *mem_record = nullptr;
	int mem_record_index = 0;
	int mem_record_capacity = 0;
	size_t initial_mem_usage = 0;
	size_t peak_mem_usage = 0;
	
	if (keep_mem_hist)
	{
		mem_record_capacity = 16384;
		mem_record = (size_t *)malloc(mem_record_capacity * sizeof(size_t));
		if (!mem_record)
			EIDOS_TERMINATION << std::endl << "ERROR (main): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
	}
	
	if (keep_mem)
	{
		// note we subtract the size of our memory-tracking buffer, here and below
		initial_mem_usage = Eidos_GetCurrentRSS() - mem_record_capacity * sizeof(size_t);
	}
	
	// run the simulation
	Eidos_WarmUp();
	SLiM_WarmUp();
	
	Community *community = nullptr;
	std::string model_name;
	
	if (!input_file)
	{
		// no input file supplied; either the user forgot (if stdin is a tty) or they're piping a script into stdin
		// we checked for the tty case above, so here we assume stdin will supply the script
		community = new Community(std::cin);
		model_name = "stdin";
	}
	else
	{
		// BCH 1/18/2020: check that input_file is a valid path to a file that we can access before opening it
		{
			FILE *fp = fopen(input_file, "r");
			
			if (!fp)
				EIDOS_TERMINATION << std::endl << "ERROR (main): could not open input file: " << input_file << "." << EidosTerminate();
			
			struct stat fileInfo;
			int retval = fstat(fileno(fp), &fileInfo);
			
			if (retval != 0)
				EIDOS_TERMINATION << std::endl << "ERROR (main): could not access input file: " << input_file << "." << EidosTerminate();
			
			// BCH 30 March 2020: adding S_ISFIFO() as a permitted file type here, to re-enable redirection of input
			if (!S_ISREG(fileInfo.st_mode) && !S_ISFIFO(fileInfo.st_mode))
			{
				fclose(fp);
				EIDOS_TERMINATION << std::endl << "ERROR (main): input file " << input_file << " is not a regular file (it might be a directory or other special file)." << EidosTerminate();
			}
			fclose(fp);
		}
		
		// input file supplied; open it and use it
		std::ifstream infile(input_file);
		
		if (!infile.is_open())
			EIDOS_TERMINATION << std::endl << "ERROR (main): could not open input file: " << input_file << "." << EidosTerminate();
		
		community = new Community(infile);
		model_name = Eidos_LastPathComponent(std::string(input_file));
	}
	
	if (keep_mem_hist)
		mem_record[mem_record_index++] = Eidos_GetCurrentRSS() - mem_record_capacity * sizeof(size_t);
	
	if (community)
	{
		community->InitializeRNGFromSeed(override_seed_ptr);
		
		Eidos_DefineConstantsFromCommandLine(defined_constants);	// do this after the RNG has been set up
		
		for (int arg_index = 0; arg_index < argc; ++arg_index)
		community->cli_params_.emplace_back(argv[arg_index]);
		
		if (tree_seq_checks)
			community->AllSpecies_TSXC_Enable();
        if (tree_seq_force && !tree_seq_checks)
			community->AllSpecies_TSF_Enable();
		
#if DO_MEMORY_CHECKS
		// We check memory usage at the end of every 10 ticks, to be able to provide the user with a decent error message
		// if the maximum memory limit is exceeded.  Every 10 ticks is a compromise; these checks do take a little time.
		// Even with a model that runs through ticks very quickly, though, checking every 10 makes little difference.
		// Models in which the ticks take longer will see no measurable difference in runtime at all.  Note that these
		// checks can be disabled with the -x command-line option.
		int mem_check_counter = 0, mem_check_mod = 10;
#endif
		
		// Run the simulation to its natural end
#if (SLIMPROFILING == 1)
		bool profiling_started = false;
		bool wrote_profile_report = false;
#endif
		
		while (true)
		{
			bool tick_result;
			
#if (SLIMPROFILING == 1)
			if (!profiling_started && (community->Tick() == profile_start_tick))
			{
				community->StartProfiling();
				profiling_started = true;
			}
			
			if (profiling_started)
			{
				std::clock_t startCPUClock = std::clock();
				SLIM_PROFILE_BLOCK_START();
				
				tick_result = community->RunOneTick();
				
				SLIM_PROFILE_BLOCK_END(community->profile_elapsed_wall_clock);
				std::clock_t endCPUClock = std::clock();
				
				community->profile_elapsed_CPU_clock += (endCPUClock - startCPUClock);
			}
			else
			{
				tick_result = community->RunOneTick();
			}
			
			if (profiling_started && ((!tick_result) || (community->Tick() == profile_end_tick)))
			{
				community->StopProfiling();
				profiling_started = false;
				
				WriteProfileResults(profile_output_path, model_name, community);
				wrote_profile_report = true;
				
				// terminate at the end of this tick, since the goal was to produce a profile
				tick_result = false;
			}
#else
			tick_result = community->RunOneTick();
#endif
			
			if (!tick_result)
				break;
			
			if (keep_mem_hist)
			{
				if (mem_record_index == mem_record_capacity)
				{
					mem_record_capacity <<= 1;
					mem_record = (size_t *)realloc(mem_record, mem_record_capacity * sizeof(size_t));
					if (!mem_record)
						EIDOS_TERMINATION << std::endl << "ERROR (main): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate();
				}
				
				mem_record[mem_record_index++] = Eidos_GetCurrentRSS() - mem_record_capacity * sizeof(size_t);
			}
			
#if DO_MEMORY_CHECKS
			if (eidos_do_memory_checks)
			{
				mem_check_counter++;
				
				if (mem_check_counter % mem_check_mod == 0)
				{
					// Check memory usage at the end of the ticks, so we can print a decent error message
					std::ostringstream message;
					
					message << "(Limit exceeded at end of tick " << community->Tick() << ".)" << std::endl;
					
					Eidos_CheckRSSAgainstMax("main()", message.str());
				}
			}
#endif
		}
		
#if (SLIMPROFILING == 1)
		// We write the profile report path at end, so it doesn't get lost in the middle of the output
		if (wrote_profile_report)
		{
			std::cerr << std::endl << "// profiled from tick " << community->profile_start_tick << " to " << community->profile_end_tick << std::endl;
			std::cerr << "// wrote profile results to " << profile_output_path << std::endl << std::endl;
		}
#endif
		
		// clean up; but most of this is an unnecessary waste of time in the command-line context
		Eidos_FlushFiles();
		
#if SLIM_LEAK_CHECKING
		delete sim;
		sim = nullptr;
		clean_up_leak_false_positives();
		
		// sleep() to give time to assess leaks at the command line
		std::cout << "\nSLEEPING" << std::endl;
		sleep(60);
#endif
	}
	
	// end timing and print elapsed time
	std::clock_t end_cpu = std::clock();
	std::chrono::steady_clock::time_point end_wall = std::chrono::steady_clock::now();
	double cpu_time_secs = static_cast<double>(end_cpu - begin_cpu) / CLOCKS_PER_SEC;
	double wall_time_secs = std::chrono::duration<double>(end_wall - begin_wall).count();
	
	if (keep_time)
	{
		SLIM_ERRSTREAM << "// ********** CPU time used: " << cpu_time_secs << std::endl;
		SLIM_ERRSTREAM << "// ********** Wall time used: " << wall_time_secs << std::endl;
	}
	
	// print memory usage stats
	if (keep_mem)
	{
		peak_mem_usage = Eidos_GetPeakRSS();
		
		SLIM_ERRSTREAM << "// ********** Initial memory usage: " << initial_mem_usage << " bytes (" << initial_mem_usage / 1024.0 << "K, " << initial_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
		SLIM_ERRSTREAM << "// ********** Peak memory usage: " << peak_mem_usage << " bytes (" << peak_mem_usage / 1024.0 << "K, " << peak_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
	}
	
	if (keep_mem_hist)
	{
		SLIM_ERRSTREAM << "// ********** Memory usage history (execute in R for a plot): " << std::endl;
		SLIM_ERRSTREAM << "memhist <- c(" << std::endl;
		for (int hist_index = 0; hist_index < mem_record_index; ++hist_index)
		{
			SLIM_ERRSTREAM << "   " << mem_record[hist_index];
			if (hist_index < mem_record_index - 1)
				SLIM_ERRSTREAM << ",";
			SLIM_ERRSTREAM << std::endl;
		}
		SLIM_ERRSTREAM << ")" << std::endl;
		SLIM_ERRSTREAM << "initial_mem <- " << initial_mem_usage << std::endl;
		SLIM_ERRSTREAM << "peak_mem <- " << peak_mem_usage << std::endl;
		SLIM_ERRSTREAM << "#scale <- 1; scale_tag <- \"bytes\"" << std::endl;
		SLIM_ERRSTREAM << "#scale <- 1024; scale_tag <- \"K\"" << std::endl;
		SLIM_ERRSTREAM << "scale <- 1024 * 1024; scale_tag <- \"MB\"" << std::endl;
		SLIM_ERRSTREAM << "#scale <- 1024 * 1024 * 1024; scale_tag <- \"GB\"" << std::endl;
		SLIM_ERRSTREAM << "plot(memhist / scale, type=\"l\", ylab=paste0(\"Memory usage (\", scale_tag, \")\"), xlab=\"Tick (start)\", ylim=c(0,peak_mem/scale), lwd=4)" << std::endl;
		SLIM_ERRSTREAM << "abline(h=peak_mem/scale, col=\"red\")" << std::endl;
		SLIM_ERRSTREAM << "abline(h=initial_mem/scale, col=\"blue\")" << std::endl;
		
		free(mem_record);
	}
	
	return EXIT_SUCCESS;
}




































































