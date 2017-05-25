/*
** Taiga
** Copyright (C) 2010-2017, Eren Okka
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <windows/win/common_dialogs.h>
#include <windows/win/version.h>
#include <utf8proc/utf8proc.h>
#include "base/log.h"
#include <windows/win/task_dialog.h>
#include "base/gfx.h"
#include "base/string.h"
#include "library/anime_db.h"
#include "library/anime_filter.h"
#include "library/anime_util.h"
#include "sync/sync.h"
#include "taiga/resource.h"
#include "taiga/script.h"
#include "taiga/settings.h"
#include "track/recognition.h"
#include "ui/dlg/dlg_main.h"
#include "ui/dialog.h"
#include "ui/list.h"
#include "ui/menu.h"
#include <utf8proc/utf8proc.h>
#include "ui/theme.h"
#include "ui/ui.h"
#include "ui/dlg/dlg_anime_playlist.h"
#include "sync/service.h"
#include "taiga/taiga.h"
#include "taiga/timer.h"
#include "track/recognition.h"
#include "ui/list.h"
#include <chrono>
#include <thread>
#include "atlstr.h"
#include <iostream>
#include <fstream>
#include <string>
#include "track/search.h"
#include "taiga/taiga.h"
#include "taiga/http.h"
#include "taiga/path.h"
#include "taiga/settings.h"
#include <locale>
#include <codecvt>

// extern ATL::CTrace TRACE;
#define TRACE ATLTRACE
using namespace std;

namespace ui {

	AnimePlaylistDialog DlgPlaylist;


	BOOL AnimePlaylistDialog::OnInitDialog() {
		// Create tab control
		
		listview.parent = this;

		list_.Attach(GetDlgItem(IDC_LIST_PLAYLIST));
		list_.SetExtendedStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP | LVS_EDITLABELS);
		list_.SetImageList(ui::Theme.GetImageList16().GetHandle());
		list_.SetTheme();
		list_.InsertColumn(0, ScaleX(400), ScaleX(400), LVCFMT_LEFT, L"Anime title");
		list_.InsertColumn(1, ScaleX(60), ScaleX(60), LVCFMT_CENTER, L"Episode");
		list_.InsertColumn(2, ScaleX(60), ScaleX(60), LVCFMT_CENTER, L"Type");
		list_.InsertColumn(3, ScaleX(60), ScaleX(60), LVCFMT_RIGHT, L"Episodes");
		list_.InsertColumn(4, ScaleX(60), ScaleX(60), LVCFMT_RIGHT, L"Score");
		list_.InsertColumn(5, ScaleX(100), ScaleX(100), LVCFMT_RIGHT, L"Season");
		


		list_.InsertGroup(1, L"Playlist (Drag and Drop Shows to Rearrange)");

		list_.EnableGroupView(true);
		LoadSavedUserPlaylist();
		return TRUE;
	}
	INT_PTR AnimePlaylistDialog::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {

			// Forward mouse wheel messages to the list
		case WM_MOUSEWHEEL: {
			return list_.SendMessage(uMsg, wParam, lParam);
		}
							break;

		case WM_DROPFILES: {
			HDROP hDropInfo = (HDROP)wParam;
			char sItem[MAX_PATH];
			
			TRACE("\nFILE DROPPED ONTO OUR THING\n");
		}
							break;


		case WM_MOUSEMOVE: {
			// Drag list item
			if (listview.dragging) {
				bool allow_drop = false;

				if (!allow_drop) {
					POINT pt;
					GetCursorPos(&pt);
					win::Rect rect_edit;
					DlgMain.edit.GetWindowRect(&rect_edit);
					if (rect_edit.PtIn(pt))
						allow_drop = true;
				}

				if (!allow_drop) {
					TVHITTESTINFO ht = { 0 };
					DlgMain.treeview.HitTest(&ht, true);
					if (ht.flags & TVHT_ONITEM) {
						int index = DlgMain.treeview.GetItemData(ht.hItem);
						switch (index) {
						case kSidebarItemSearch:
						case kSidebarItemFeeds:
							allow_drop = true;
							break;
						}
					}
				}

				POINT pt;
				GetCursorPos(&pt);
				::ScreenToClient(DlgMain.GetWindowHandle(), &pt);
				win::Rect rect_header;
				::GetClientRect(listview.GetHeader(), &rect_header);
				listview.drag_image.DragMove(pt.x + (GetSystemMetrics(SM_CXCURSOR) / 2),
					pt.y + rect_header.Height());
				SetSharedCursor(IDC_ARROW);
			}
		}
						   break;

		case WM_LBUTTONUP: {
			// Drop list item
			if (listview.dragging) {
				listview.drag_image.DragLeave(GetWindowHandle());
				listview.drag_image.EndDrag();
				listview.drag_image.Destroy();
				listview.dragging = false;
				ReleaseCapture();

				int anime_idcur = GetCurrentId();
				auto anime_ids = GetCurrentIds();
				if (anime_ids.size() > 1) return 0;
				auto anime_item = GetCurrentItem();

				lvhti.pt.x = LOWORD(lParam);
				lvhti.pt.y = HIWORD(lParam);

				ClientToScreen(DlgMain.GetWindowHandle(), &lvhti.pt);
				ScreenToClient(GetWindowHandle(), &lvhti.pt);
				lvhti.iItem = list_.HitTest();
				if (lvhti.iItem == -1) {
					TRACE("\nOutside of List!\n", lvhti.iItem);
					return 0;
				}
				if (((lvhti.flags & LVHT_ONITEMLABEL == 0)) && ((lvhti.flags & LVHT_ONITEMSTATEICON == 0)))
				{
					TRACE("\nNOT IN AN ITEM\n", lvhti.iItem);
					return 0;
				}

				LVITEM lvswag;
				lvswag.iItem = lvhti.iItem;
				lvswag.iSubItem = 0;
				lvswag.mask = LVIF_STATE;
				lvswag.stateMask = LVIS_SELECTED;
				
				int iPos = list_.GetNextItem(-1, LVNI_SELECTED);

				if (lvhti.iItem == iPos)
				{
					return 0;
				}

				int droppedID = list_.GetItemParam(lvhti.iItem);
				if (droppedID == anime_idcur)
				{
					return 0;
				}
				int flagboy = 0;

				if (iPos < lvhti.iItem)
				{
					TRACE("ACCOMODATE FOR THIS SITUATION");
					flagboy = 1;
				}

					while (iPos != -1)
					{
						LVITEM lvi;
						lvi.iItem = iPos;
						lvi.iSubItem = 0;
						lvi.cchTextMax = INFOTIPSIZE;

						LPCWSTR text = L"Test String";

						lvi.pszText = const_cast<LPWSTR>(text);
						lvi.stateMask = ~LVIS_SELECTED;
						lvi.mask = LVIF_STATE | LVIF_IMAGE
							| LVIF_INDENT | LVIF_PARAM | LVIF_TEXT;

						list_.GetItemParam(lvi.iItem);

						lvi.iItem = lvhti.iItem;

						int iRet = list_.InsertItem(lvhti.iItem+flagboy, 1,
							StatusToIcon(anime_item->GetAiringStatus()), 0, nullptr,
							anime_item->GetTitle().c_str(),
							static_cast<LPARAM>(anime_item->GetId()));

						if (lvi.iItem < iPos) lvhti.iItem++;

						if (iRet < iPos) iPos++;


						std::wstring bad = ToWstr(anime_item->GetMyLastWatchedEpisode());
						
						int h = std::stoi(bad)+1; // convert to int

						std::wstring next = std::to_wstring(h); //nextepisode

						list_.SetItem(iRet, 1, next.c_str());
						list_.SetItem(iRet, 2, anime::TranslateType(anime_item->GetType()).c_str());
						list_.SetItem(iRet, 3, anime::TranslateNumber(anime_item->GetEpisodeCount()).c_str());
						list_.SetItem(iRet, 4, anime::TranslateScore(anime_item->GetScore()).c_str());
						list_.SetItem(iRet, 5, anime::TranslateDateToSeasonString(anime_item->GetDateStart()).c_str());

						list_.DeleteItem(iPos);
						iPos = list_.GetNextItem(-1, LVNI_SELECTED);

					}
					list_.SetRedraw(FALSE);
					std::vector<int> new_anime_ids = GetCurrentIds();
					new_anime_ids.clear();
					int counter = 0;
					for (const auto& anime_id : anime_ids_)
					{
						int curnid = list_.GetItemParam(counter);
						auto new_item = AnimeDatabase.FindItem(anime_id);
						new_anime_ids.push_back(curnid);
						counter++;
					}
					list_.DeleteAllItems();
					for (const auto& anime_id : new_anime_ids )
					{
							AddAnimeToList(anime_id);
					}
					list_.SetRedraw(TRUE);
					list_.InvalidateRect();
					list_.Update();
			}
			break;
		}
		}
		return DialogProcDefault(hwnd, uMsg, wParam, lParam);
	}
	void AnimePlaylistDialog::OnContextMenu(HWND hwnd, POINT pt) {
		if (hwnd == list_.GetWindowHandle()) {
			if (list_.GetSelectedCount() > 0) {
				if (pt.x == -1 || pt.y == -1)
					GetPopupMenuPositionForSelectedListItem(list_, pt);
				auto anime_id = GetAnimeIdFromSelectedListItem(list_);
				auto anime_ids = GetAnimeIdsFromSelectedListItems(list_);
				bool is_in_list = true;
				for (const auto& anime_id : anime_ids) {
					auto anime_item = AnimeDatabase.FindItem(anime_id);
					if (anime_item && !anime_item->IsInList()) {
						is_in_list = false;
						break;
					}
				}
				ui::Menus.UpdateSearchList(!is_in_list);
				std::wstring menu_name = L"PlayListList";
				LPARAM parameter = GetCurrentId();
				auto action = ui::Menus.Show(DlgMain.GetWindowHandle(), pt.x, pt.y, menu_name.c_str());


				if (action == L"EditDelete()") {
					//list_.DeleteAllItems();
					for (const auto& anime_id : anime_ids) {
						int iPos = list_.GetNextItem(-1, LVNI_SELECTED);
						if (iPos != -1) {
							list_.DeleteItem(iPos);
							iPos = list_.GetNextItem(iPos, LVNI_SELECTED);
						}

					}
					exportplaylisttofile(L"yes");

				}
				else if (action == L"PlayList()") {
					parameter = reinterpret_cast<LPARAM>(&anime_ids_);
					int current_position = 0;


					std::wstring path = taiga::GetPath(taiga::Path::kPathUserSavedPlaylist);
					std::wofstream f(path, std::ios::binary);
					f << "#EXTM3U\n";
					int listsize = list_.GetItemCount();
					TRACE("LIST AMOUNT: %d\n", listsize);
					while (current_position < list_.GetItemCount()) {

						int anime_id = list_.GetItemParam(current_position);
						auto anime_item = AnimeDatabase.FindItem(anime_id);
						bool found = false;
						file_search_helper.set_anime_id(anime_id);

						file_search_helper.set_minimum_file_size(
						Settings.GetInt(taiga::kLibrary_FileSizeThreshold));
						file_search_helper.set_path_found(L"");

						anime::ValidateFolder(*anime_item);

						if (!anime_item->GetFolder().empty()) {
							file_search_helper.set_skip_directories(true);
							file_search_helper.set_skip_files(false);
							file_search_helper.set_skip_subdirectories(false);
							found = file_search_helper.Search(anime_item->GetFolder());

							int duration = anime_item->GetEpisodeLength();
							std::wstring dur = std::to_wstring(duration);
							const std::wstring& next_episode_path = anime_item->GetNextEpisodePath();


							const std::wstring& title = anime_item->GetTitle();
							//const std::wstring& title = title.c_str();


							std::string utf8str;
							std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
							for (auto &ch : title) utf8str.append(utf8_conv.to_bytes(ch));



							f << "#EXTINF:" << dur.c_str() << "," << utf8str.c_str() << "\n" << next_episode_path.c_str() << "\n";
							ui::ChangeStatusText(L"Added " + next_episode_path + L" to playlist.");



						}
						else {

							ui::ChangeStatusText(L"Couldn't Find File for " + anime_item->GetTitle() + L"!");

						}

						current_position++;

					}
					std::this_thread::sleep_for(std::chrono::milliseconds(3000));
					f.close();
					ui::ChangeStatusText(L"Playing List");
					Execute(path);
						
				}

				else if (action == L"Info()") {
					parameter = reinterpret_cast<LPARAM>(&anime_ids);
					for (const auto& anime_id : anime_ids) {
						ui::ShowDlgAnimeInfo(anime_id);
						break;
					}
				}

				else if (action == L"OpenFolder()")
				{
					parameter = reinterpret_cast<LPARAM>(&anime_ids);
					for (const auto& anime_id : anime_ids) {
						ExecuteAction(L"OpenFolder", 0, anime_id);
						break;
					}
				}
				else if (action == L"ExportPlaylist()")
					{

					exportplaylisttofile(L"");


					}

				else if (action == L"ScheduleList()")
					{
						parameter = reinterpret_cast<LPARAM>(&anime_ids);
						


						std::wstring badboy = L"at 07:00 cmd ";


						badboy.append(taiga::GetPath(taiga::Path::kPathUserSavedPlaylist));
			

						std::string path(badboy.begin(), badboy.end());


						WinExec(path.c_str(), SW_HIDE);

					}

			}
		}
	}
	void AnimePlaylistDialog::SchedulePlayListDate()
	{

	}

	void  AnimePlaylistDialog::exportplaylisttofile(const std::wstring&  directoryname) //includes no directory in case of user export. but assumed if its the auto playlist
	{
		LPARAM parameter = GetCurrentId();
		parameter = reinterpret_cast<LPARAM>(&anime_ids_);
		int current_position = 0;
		//const std::wstring path = L"C:/damn.m3u";


		std::wstring path = taiga::GetPath(taiga::Path::kPathUserSavedPlaylist);

		if (directoryname.empty())
		{
			path = PlayListFolderDestination();
			TRACE("\nNO DIRECTORY PROVIDED. USER CHOOSES SAVE LOCATION!!\n");
			
		}
		else {
			TRACE("\nWE WANT TO SAVE THIS PLAYLIST IN THE kPathUserSavedPlaylist!\n");

		}
		

		if (path.empty()) return;

		TRACE(L"\n\nTHE PATH IS %s\n\n", path);

		std::wofstream f(path, std::ios::binary);
		f << "#EXTM3U\n";
		int listsize = list_.GetItemCount();
		TRACE("LIST AMOUNT: %d\n", listsize);
		while (current_position < list_.GetItemCount()) {

			int anime_id = list_.GetItemParam(current_position);
			auto anime_item = AnimeDatabase.FindItem(anime_id);
			bool found = false;
			file_search_helper.set_anime_id(anime_id);

			file_search_helper.set_minimum_file_size(
				Settings.GetInt(taiga::kLibrary_FileSizeThreshold));
			file_search_helper.set_path_found(L"");

			anime::ValidateFolder(*anime_item);

			if (!anime_item->GetFolder().empty()) {
				file_search_helper.set_skip_directories(true);
				file_search_helper.set_skip_files(false);
				file_search_helper.set_skip_subdirectories(false);
				found = file_search_helper.Search(anime_item->GetFolder());

				int duration = anime_item->GetEpisodeLength();
				TRACE("\nduration is %d:\n", duration);
				std::wstring dur = std::to_wstring(duration);
				const std::wstring& next_episode_path = anime_item->GetNextEpisodePath();


				const std::wstring& title = anime_item->GetTitle();
				//const std::wstring& title = title.c_str();


				std::string utf8str;
				std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
				for (auto &ch : title) utf8str.append(utf8_conv.to_bytes(ch));



				f << "#EXTINF:" << dur.c_str() << "," << utf8str.c_str() << "\n" << next_episode_path.c_str() << "\n";
				ui::ChangeStatusText(L"Added " + next_episode_path + L" to playlist.");



			}
			else {

				ui::ChangeStatusText(L"Couldn't Find File for " + anime_item->GetTitle() + L"!");

			}

			current_position++;

		}
		f.close();
		ui::ChangeStatusText(L"Saved List");

	}












	LRESULT AnimePlaylistDialog::OnNotify(int idCtrl, LPNMHDR pnmh) {
		// ListView control
		if (idCtrl == IDC_LIST_PLAYLIST) {
			switch (pnmh->code) {

			case LVN_BEGINDRAG: {
				auto lplv = reinterpret_cast<LPNMLISTVIEW>(pnmh);
				listview.drag_image.Duplicate(ui::Theme.GetImageList16().GetHandle());
if (listview.drag_image.GetHandle()) {
	int icon_index = ui::kIcon16_DocumentA;
	int anime_id = list_.GetItemParam(lplv->iItem);
	//TRACE("ANIME ID:%d\n", anime_id);
	auto anime_item = AnimeDatabase.FindItem(anime_id);

	if (anime_item)
	{
		//TRACE("IS AN ANIME ITEM\n");
		icon_index = ui::StatusToIcon(anime_item->GetAiringStatus());
	}
	listview.drag_image.BeginDrag(icon_index, 0, 0);
	listview.drag_image.DragEnter(DlgMain.GetWindowHandle(),
		lplv->ptAction.x, lplv->ptAction.y);
	listview.dragging = true;
	SetCapture();
}
break;


			}



			// Key press
			case LVN_KEYDOWN: {
				auto pnkd = reinterpret_cast<LPNMLVKEYDOWN>(pnmh);
				switch (pnkd->wVKey) {
				case VK_RETURN: {
					int anime_id = GetAnimeIdFromSelectedListItem(list_);
					if (anime::IsValidId(anime_id)) {
						ShowDlgAnimeInfo(anime_id);
						return TRUE;
					}
					break;
				}
				}
				break;
			}

							  // Double click
			case NM_DBLCLK: {
				LPNMITEMACTIVATE lpnmitem = reinterpret_cast<LPNMITEMACTIVATE>(pnmh);
				if (lpnmitem->iItem > -1) {
					int anime_id = list_.GetItemParam(lpnmitem->iItem);
					if (anime::IsValidId(anime_id))
						ShowDlgAnimeInfo(anime_id);
				}
				break;
			}
			}
		}

		return 0;
	}

	void AnimePlaylistDialog::OnSize(UINT uMsg, UINT nType, SIZE size) {
		switch (uMsg) {
		case WM_SIZE: {
			win::Rect rcWindow;
			rcWindow.Set(0, 0, size.cx, size.cy);
			// Resize list
			list_.SetPosition(nullptr, rcWindow);
		}
		}
	}

	static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
	{

		if (uMsg == BFFM_INITIALIZED)
		{
			std::string tmp = (const char *)lpData;
			std::cout << "path: " << tmp << std::endl;
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
		}

		return 0;
	}

	
	const std::wstring  AnimePlaylistDialog::PlayListFolderDestination() {

		OPENFILENAME ofn;

		WCHAR szFileName[MAX_PATH] = L"";
		

		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFilter = (LPCWSTR)L".m3u Playlist Format (*.m3u)";
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrDefExt = (LPCWSTR)L"m3u";


		GetSaveFileName(&ofn);


		TRACE(L"the path is : %s\n", szFileName);
		getchar();
		
		return szFileName;
			/*std::wstring default_path, path;
			if (!Settings.library_folders.empty())
				default_path = Settings.library_folders.front();
			if (win::BrowseForFolder(ui::GetWindowHandle(ui::kDialogMain),
				L"Select Playlist Save Location",
				default_path, path)) {
				TRACE("\nSAVE PATH IS: %s\n",path);
				return path;

			}
			TRACE("\nDIDNT PICK UP ANY PATH\n");
			return L"";
			*/
	}

	////////////////////////////////////////////////////////////////////////////////
	void AnimePlaylistDialog::AddAnimeToList(int anime_id) {

		auto anime_item = AnimeDatabase.FindItem(anime_id);
		if (anime_item) {
			int i = list_.GetItemCount();
			list_.InsertItem(i, anime_item->IsInList() ? 1 : 0,
				StatusToIcon(anime_item->GetAiringStatus()), 0, nullptr,
				anime_item->GetTitle().c_str(),
				static_cast<LPARAM>(anime_item->GetId()));
			std::wstring bad = ToWstr(anime_item->GetMyLastWatchedEpisode());
			int h = std::stoi(bad);
			if
				(!(anime_item->GetMyLastWatchedEpisode() == anime_item->GetEpisodeCount()))

		  {
				h = std::stoi(bad) + 1; // convert to int
			}

			std::wstring next = std::to_wstring(h); //nextepisode


			list_.SetItem(i, 1, next.c_str());
			list_.SetItem(i, 2, anime::TranslateType(anime_item->GetType()).c_str());
			list_.SetItem(i, 3, anime::TranslateNumber(anime_item->GetEpisodeCount()).c_str());
			list_.SetItem(i, 4, anime::TranslateScore(anime_item->GetScore()).c_str());
			list_.SetItem(i, 5, anime::TranslateDateToSeasonString(anime_item->GetDateStart()).c_str());
		}
	}

	void AnimePlaylistDialog::ParsePlaylistFile(const std::wstring&  playlistfile) {

		
		TRACE(L"\nPASSED FILE: %s\n", playlistfile.c_str());
		

		std::wifstream file(playlistfile.c_str());

		if (file)
		{
			std::wstring line;
			while (std::getline(file, line))
			{
				
				if (line.find(L"EXTINF") != std::string::npos)
				{
					std::getline(file, line);
					if (line.empty()) {
						
					}
					else
					{
						line = line.substr(line.find(L",", 0) + 1, line.length()).c_str();
						std::string utf8str;
						std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
						for (auto &ch : line) utf8str.append(utf8_conv.to_bytes(ch));

						static track::recognition::ParseOptions parse_options;
						parse_options.parse_path = true;
						parse_options.streaming_media = false;

						TRACE("\nIDENTIFIED? %d\n",Meow.Parse(line, parse_options, episode_));
						static track::recognition::MatchOptions match_options;
						match_options.allow_sequels = false;
						match_options.check_airing_date = false;
						match_options.check_anime_type = false;
						match_options.check_episode_number = false;

						Meow.Identify(episode_, false, match_options);

						anime::Item* anime_item = AnimeDatabase.FindItem(episode_.anime_id);
						TRACE(L"\nANIME NAME: %s\n", anime_item->GetTitle().c_str());


						


						AddAnimeToList(episode_.anime_id);
						anime_ids_.push_back(episode_.anime_id);
						exportplaylisttofile(L"yes");
						

						//TRACE(L"%s\n", line.c_str());
					}
				}
			}
			file.close();
			ui::DlgMain.navigation.SetCurrentPage(ui::kSidebarItemPlaylist);
		}

		else TRACE("Unable to open file");





	}


	
	

	bool AnimePlaylistDialog::LoadSavedUserPlaylist() {
		std::wstring path = taiga::GetPath(taiga::Path::kPathUserSavedPlaylist);
		ParsePlaylistFile(path);
		return true;
	}

	void AnimePlaylistDialog::ParseResults(const std::vector<int>& ids) {
		ui::DlgMain.navigation.SetCurrentPage(kSidebarItemPlaylist);

		auto existing_anime_ids = GetCurrentIds();
		std::vector<int> playlistids = GetCurrentIds();
		playlistids.clear();


		int current_position = 0;
		while (current_position < list_.GetItemCount()) {
			int curnid = list_.GetItemParam(current_position);

			playlistids.push_back(curnid);
			

			current_position = current_position + 1;
		}


		//std::vector<int> existing_anime_ids = GetCurrentIds();

		if (playlistids.empty()) {
			TRACE("EMPTY");
			for (const auto& anime_id : ids)
			{
				AddAnimeToList(anime_id);
				
			}
			anime_ids_ = ids;
		}
		else {
			//TRACE("NOT EMPTY");
			//TRACE("FIRST ANIME ID: %d", existing_anime_ids[0]);
			for (const auto& anime_id : ids)
			{


				if (std::find(playlistids.begin(), playlistids.end(), anime_id) != playlistids.end())
				{

					TRACE("FOUND IT IN LIST!!\n");

				}
				else
				{
					AddAnimeToList(anime_id);
					anime_ids_.push_back(anime_id);

					TRACE("IT DOESN'T EXIST IN THERE\n");
				}


			}
		}

		//anime_ids_ = ids;

		//RefreshList();

	}
	void AnimePlaylistDialog::PushBack(int anime_id)
	{
		//AddAnimeToList(anime_id);
		//anime_ids_.push_back(anime_id);

		auto existing_anime_ids = GetCurrentIds();


		std::vector<int> playlistids = GetCurrentIds();
		playlistids.clear();


		int current_position = 0;
		while (current_position < list_.GetItemCount()) {
			int curnid = list_.GetItemParam(current_position);

			playlistids.push_back(curnid);


			current_position = current_position + 1;
		}

		if (playlistids.empty()) {
			TRACE("EMPTY");

				AddAnimeToList(anime_id);
				anime_ids_.push_back(anime_id);

		}
		else {

				if (std::find(playlistids.begin(), playlistids.end(), anime_id) != playlistids.end())
				{

					TRACE("FOUND IT IN LIST!!\n");

				}
				else
				{
					AddAnimeToList(anime_id);
					anime_ids_.push_back(anime_id);

					TRACE("IT DOESN'T EXIST IN THERE\n");
				}
			
		}

	}



	void AnimePlaylistDialog::RefreshList() {
		if (!IsWindow())
			return;

		// Disable drawing
		list_.SetRedraw(FALSE);



		std::vector<int> new_anime_ids = GetCurrentIds();
		new_anime_ids.clear();
		int counter = 0;
		for (const auto& anime_id : anime_ids_)
		{
			int curnid = list_.GetItemParam(counter);
			auto new_item = AnimeDatabase.FindItem(anime_id);
			new_anime_ids.push_back(curnid);
			counter++;
		}

		// Clear list
		list_.DeleteAllItems();
		// Add anime items to list

		for (const auto& anime_id : new_anime_ids)
		{
			AddAnimeToList(anime_id);
		}
		exportplaylisttofile(L"yes");
		// Redraw
		list_.SetRedraw(TRUE);
		list_.RedrawWindow(nullptr, nullptr,
			RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
	}


	

	AnimePlaylistDialog::ListView::ListView()
		: dragging(false), hot_item(-1), parent(nullptr), progress_bars_visible(false) {
		button_visible[0] = false;
		button_visible[1] = false;
		button_visible[2] = false;
	}
	int AnimePlaylistDialog::GetCurrentId() {
		return GetAnimeIdFromSelectedListItem(list_);
	}

	std::vector<int> AnimePlaylistDialog::GetCurrentIds() {
		return GetAnimeIdsFromSelectedListItems(list_);
	}

	anime::Item* AnimePlaylistDialog::GetCurrentItem() {
		return AnimeDatabase.FindItem(GetCurrentId());
	}

	int AnimePlaylistDialog::GetCurrentStatus() {
		return current_status_;
	}

	int AnimePlaylistDialog::GetListIndex(int anime_id) {
		auto it = listview.id_cache.find(anime_id);
		return it != listview.id_cache.end() ? it->second : -1;
	}
	void AnimePlaylistDialog::RebuildIdCache() {
		listview.id_cache.clear();
		for (int i = 0; i < listview.GetItemCount(); i++)
			listview.id_cache[listview.GetItemParam(i)] = i;
	}

	LRESULT AnimePlaylistDialog::ListView::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {

			// Mouse leave
		case WM_MOUSELEAVE: {
			int item_index = GetNextItem(-1, LVIS_SELECTED);
			if (item_index != hot_item)
				//RefreshItem(-1);
				break;
		}

							// Set cursor
		case WM_SETCURSOR: {
			POINT pt;
			::GetCursorPos(&pt);
			::ScreenToClient(GetWindowHandle(), &pt);
			if ((button_visible[0] && button_rect[0].PtIn(pt)) ||
				(button_visible[1] && button_rect[1].PtIn(pt)) ||
				(button_visible[2] && button_rect[2].PtIn(pt))) {
				SetSharedCursor(IDC_HAND);
				return TRUE;
			}
			break;
		}
		}

		return WindowProcDefault(hwnd, uMsg, wParam, lParam);
	}



}  // namespace ui