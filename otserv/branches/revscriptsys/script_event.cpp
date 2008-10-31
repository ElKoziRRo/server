//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#include "otpch.h"

#include "script_event.h"
#include "script_enviroment.h"
#include "script_listener.h"
#include "script_manager.h"
#include "tools.h"

#include "creature.h"

uint32_t Script::Event::eventID_counter = 0;

using namespace Script;

Event::Event() : eventID(++eventID_counter) {
	std::ostringstream os;
	os << "EI_" << eventID; // Event instance tag
	lua_tag = os.str();
}

Event::~Event() {
}

///////////////////////////////////////////////////////////////////////////////C

template<class T, class ScriptInformation>
bool dispatchEvent(T* e, Manager& state, Enviroment& enviroment, ListenerList& specific_list) {
	if(specific_list.size() == 0) {
		return false;
	}
	for(ListenerList::iterator event_iter = specific_list.begin();
		event_iter != specific_list.end();
		++event_iter)
	{
		Listener_ptr listener = *event_iter;
		if(listener->isActive() == false) continue;
		const ScriptInformation& info = boost::any_cast<const ScriptInformation>(listener->getData());

		// Call handler
		if(e->check_match(info)) {
			if(e->call(state, enviroment, listener) == true) {
				// Handled
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

OnSay::Event::Event(Creature* _speaker, SpeakClass& _type, std::string& _receiver, std::string& _class) :
	speaker(_speaker),
	speak_class(_class),
	receiver(_receiver),
	text(_text)
{
}

OnSay::Event::~Event() {
}

bool OnSay::Event::check_match(const ScriptInformation& info) {
	std::string match_text = text;
	if(!info.case_sensitive) {
		toLowerCaseString(match_text);
	}

	switch(info.method) {
		case FILTER_ALL: 
			return true;
		case FILTER_SUBSTRING:
			return match_text.find(info.filter) != std::string::npos;
		case FILTER_MATCH_BEGINNING:
			return match_text.find(info.filter) == 0;
		case FILTER_EXACT:
			return match_text == info.filter;
		default: break;
	}
	return false;
}

bool OnSay::Event::dispatch(Manager& state, Enviroment& enviroment) {
	if(dispatchEvent<OnSay::Event, ScriptInformation>(this, state, enviroment, speaker->getListeners(ONSAY_LISTENER)))
		return true;

	return dispatchEvent<OnSay::Event, ScriptInformation>(this, state, enviroment, enviroment.Generic.OnSay);
}

bool Event::call(Manager& stae, Enviroment& enviroment, Listener_ptr listener) {
	LuaThread_ptr thread = state.newThread("OnSay");

	// Stack is empty
	// Push callback
	thread->pushCallback(listener);
	
	if(thread->isNil()) {
		thread->HandleError("Attempt to call destroyed '" + name() + "' listener.");
		thread->pop();
		return false;
	}

	// Push event
	thread->pushEvent(*this);
	thread->duplicate();
	thread->setRegistryItem(lua_tag);
	

	// Run thread
	thread->run(1);

	if(thread->ok() == false) {
		state.pushNil();
		state.setRegistryItem(lua_tag);
		return false;
	}

	// Retrieve event info
	thread->getRegistryItem(lua_tag);
	
	// Update this instance with values from lua
	update_instance(state, enviroment, thread);
	
	// clean up
	thread->pushNil();
	thread->setRegistryItem(lua_tag);
	thread->pop(); // pop event table, will be garbage collected as we cleared the registry from it

	return true;
}

bool OnSay::Event::update_instance(Manager& state, Enviroment& enviroment, LuaThread_ptr thread) {
	thread->getField(-1, "class");
	if(thread->isNumber()) {
		speak_class = (SpeakClass)thread->popInteger();
	}
	else {
		thread->HandleError("Event 'OnSay' invalid value of 'class'");
		thread->pop();
	}

	thread->getField(-1, "receiver");
	if(thread->isString()) {
		receiver = thread->popString();
	}
	else {
		thread->HandleError("Event 'OnSay' invalid value of 'receiver'");
		thread->pop();
	}

	thread->getField(-1, "text");
	if(thread->isString()) {
		text = thread->popString();
	} 
	else {
		thread->HandleError("Event 'OnSay' invalid value of 'text'");
		thread->pop();
	}
	return true;
}

void OnSay::Event::push_instance(LuaState& state, Enviroment& enviroment) {
	//std::cout << "pushing instance" << std::endl;
	state.pushClassTableInstance("OnSayEvent");
	state.pushThing(speaker);
	state.setField(-2, "speaker");
	state.setField(-1, "class", int32_t(speak_class));
	state.setField(-1, "receiver", receiver);
	state.setField(-1, "text", text);
	//std::cout << state.typeOf() << ":" << state.getStackTop() << std::endl;
	//std::cout << "endof" << std::endl;
}
