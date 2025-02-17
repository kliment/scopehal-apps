/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of main application class
 */

#include "glscopeclient.h"
#include "../scopehal/MockOscilloscope.h"

using namespace std;

ScopeApp::~ScopeApp()
{
	ShutDownSession();
}

void ScopeApp::run(
	vector<string> filesToLoad,
	bool reconnect,
	bool nodata,
	bool retrigger,
	bool nodigital,
	bool nospectrum)
{
	register_application();

	m_window = new OscilloscopeWindow(m_scopes, nodigital, nospectrum);
	add_window(*m_window);

	//Handle file loads specified on the command line
	bool first = true;
	for(auto f : filesToLoad)
	{
		//Ignore blank files (should never happen)
		if(f.empty())
			continue;

		//Can load multiple CSVs
		if(f.find(".csv") != string::npos)
		{
			if(first)
			{
				m_window->ImportCSVToNewSession(f);
				first = false;
			}
			else
				m_window->ImportCSVToExistingSession(f);
		}

		else if(f.find(".wav") != string::npos)
		{
			if(first)
			{
				m_window->ImportWAVToNewSession(f);
				first = false;
			}
			else
				m_window->ImportWAVToExistingSession(f);
		}

		//Can only load one bin or VCD
		else if (f.find(".bin") != string::npos)
		{
			m_window->DoImportBIN(f);
			break;
		}
		else if (f.find(".vcd") != string::npos)
		{
			m_window->DoImportVCD(f);
			break;
		}

		//Can only load one scopesession
		else if (f.find(".scopesession") != string::npos)
		{
			m_window->DoFileOpen(f, true, !nodata, reconnect);
			break;
		}

		else
			LogError("Unrecognized file extension, ignoring %s\n", f.c_str());
	}

	m_window->present();

	//If no scope threads are running already (from a file load), start them now
	if(m_threads.empty())
		StartScopeThreads();

	//If retriggering, start the trigger
	if(retrigger)
		m_window->OnStart();

	while(true)
	{
		Gtk::Main::iteration();

		//Stop if the main window got closed
		if(!m_window->is_visible())
			break;
	}

	m_terminating = true;

	delete m_window;
	m_window = NULL;
}

void ScopeApp::DispatchPendingEvents()
{
	while(Gtk::Main::events_pending())
		Gtk::Main::iteration();
}

/**
	@brief Shuts down the current session and disconnects from all instruments but don't close the window
 */
void ScopeApp::ShutDownSession()
{
	//Set terminating flag so all current ScopeThread's terminate
	m_terminating = true;

	//Wait for all threads to shut down and remove them
	for(auto t : m_threads)
	{
		t->join();
		delete t;
	}
	m_threads.clear();

	//Clean up scopes
	for(auto scope : m_scopes)
		delete scope;
	m_scopes.clear();

	//Back to normal mode
	m_terminating = false;
}

void ScopeApp::StartScopeThreads()
{
	//Start the scope threads
	for(auto scope : m_scopes)
	{
		//Mock scopes can't trigger, so don't waste time polling them
		if(dynamic_cast<MockOscilloscope*>(scope) != NULL)
			continue;

		m_threads.push_back(new thread(ScopeThread, scope));
	}
}
