/*
BodySlide and Outfit Studio
Copyright (C) 2015  Caliente & ousnius
See the included LICENSE file
*/

#include "SliderCategories.h"

#pragma warning (disable: 4018)

int SliderCategoryCollection::LoadCategories(const string& basePath) {
	categories.clear();

	wxArrayString files;
	wxDir::GetAllFiles(basePath, &files, "*.xml");

	for (auto &file : files) {
		SliderCategoryFile catFile(file.ToStdString());
		vector<string> cats;
		catFile.GetCategoryNames(cats);
		for (auto &cat : cats) {
			SliderCategory sliderCat;
			catFile.GetCategory(cat, sliderCat);
			if (categories.find(cat) != categories.end())
				categories[cat].MergeSliders(sliderCat);
			else
				categories[cat] = move(sliderCat);
		}
	}

	return 0;
}

int SliderCategoryCollection::GetAllCategories(vector<string>& outCategories) {
	outCategories.clear();
	for (auto &c : categories)
		outCategories.push_back(c.first);

	return outCategories.size();
}

int SliderCategoryCollection::GetSliderCategory(const string& sliderName, string& outCategory) {
	for (auto &c : categories)
		if (c.second.HasSlider(sliderName)) {
			outCategory = c.first;
			break;
		}

	return outCategory.size();
}

int SliderCategoryCollection::GetCategorySliders(const string& categoryName, vector<string>& outSliders, bool append) {
	auto git = categories.find(categoryName);
	if (git == categories.end())
		return 0;

	if (append)
		return git->second.AppendSliders(outSliders);
	else
		return git->second.GetSliders(outSliders);
}

int SliderCategoryCollection::GetCategorySliders(const string& categoryName, unordered_set<string>& outSliders, bool append) {
	auto git = categories.find(categoryName);
	if (git == categories.end())
		return 0;

	if (append)
		return git->second.AppendSliders(outSliders);
	else
		return git->second.GetSliders(outSliders);
}

bool SliderCategoryCollection::GetCategoryHidden(const string& categoryName) {
	auto git = categories.find(categoryName);
	if (git == categories.end())
		return true;

	return git->second.GetHidden();
}

int SliderCategoryCollection::SetCategoryHidden(const string& categoryName, bool hide) {
	auto git = categories.find(categoryName);
	if (git == categories.end())
		return 1;

	git->second.SetHidden(hide);
	return 0;
}

void SliderCategory::MergeSliders(const SliderCategory& sourceCategory) {
	for (int i = 0; i < sourceCategory.sliders.size(); i++) {
		sliders.push_back(sourceCategory.sliders[i]);
		sourceFiles.push_back(sourceCategory.sourceFiles[i]);
		uniqueSourceFiles.insert(sourceCategory.sourceFiles[i]);
	}
}

int SliderCategory::LoadCategory(XMLElement* srcCategoryElement) {
	if (srcCategoryElement == nullptr)
		return 1;

	srcCategoryElement->QueryBoolAttribute("defaultHidden", &isHidden);

	name = srcCategoryElement->Attribute("name");
	XMLElement* slider = srcCategoryElement->FirstChildElement("Slider");
	while (slider) {
		string tempC = "";
		string sName = slider->Attribute("name");
		sliders.push_back(sName);
		sourceFiles.push_back(slider->GetDocument()->Value());
		uniqueSourceFiles.insert(sourceFiles.back());
		slider = slider->NextSiblingElement("Slider");
	}
	return 0;
}

bool SliderCategory::HasSlider(const string& search) {
	for (auto &m : sliders)
		if (m.compare(search) == 0)
			return true;

	return false;
}

bool SliderCategory::GetHidden() {
	return isHidden;
}

void SliderCategory::SetHidden(bool hide) {
	isHidden = hide;
}

int SliderCategory::GetSliders(vector<string>& outSliders) {
	outSliders.clear();
	outSliders.insert(outSliders.end(), sliders.begin(), sliders.end());
	return outSliders.size();
}

int SliderCategory::GetSliders(unordered_set<string>& outSliders) {
	outSliders.insert(sliders.begin(), sliders.end());
	return outSliders.size();
}

int SliderCategory::AppendSliders(vector<string>& outSliders) {
	outSliders.insert(outSliders.end(), sliders.begin(), sliders.end());
	return outSliders.size();
}

int SliderCategory::AppendSliders(unordered_set<string>& outSliders) {
	outSliders.insert(sliders.begin(), sliders.end());
	return outSliders.size();
}

int SliderCategory::AddSliders(const vector<string>& inSliders) {
	sliders.insert(sliders.end(), inSliders.begin(), inSliders.end());
	return sliders.size();
}

void SliderCategory::WriteCategory(XMLElement* categoryElement, bool append) {
	if (!append)
		categoryElement->DeleteChildren();

	for (auto &slider : sliders) {
		XMLElement* newElement = categoryElement->GetDocument()->NewElement("Slider");
		XMLElement* element = categoryElement->InsertEndChild(newElement)->ToElement();
		element->SetAttribute("name", slider.c_str());
	}
}

void SliderCategory::AddSourceFile(const string& fileName) {
	uniqueSourceFiles.insert(fileName);
}

SliderCategoryFile::SliderCategoryFile(const string& srcFileName) {
	root = nullptr;
	error = 0;
	Open(srcFileName);
}

void SliderCategoryFile::Open(const string& srcFileName) {
	if (doc.LoadFile(srcFileName.c_str()) == XML_SUCCESS) {
		fileName = srcFileName;
		root = doc.FirstChildElement("SliderCategories");
		if (!root) {
			error = 2;
			return;
		}

		XMLElement* element = root->FirstChildElement("Category");
		while (element) {
			categoriesInFile[element->Attribute("name")] = element;
			element = element->NextSiblingElement("Category");
		}
		if (categoriesInFile.empty()) {
			error = 3;
			return;
		}

	}
	else {
		error = 1;
		return;
	}
	error = 0;
}

void SliderCategoryFile::New(const string& newFileName) {
	if (root)
		return;

	doc.Clear();
	if (doc.LoadFile(newFileName.c_str()) == XML_SUCCESS) {
		error = 1;
	}
	else {
		XMLElement* newElement = doc.NewElement("SliderCategories");
		root = doc.InsertEndChild(newElement)->ToElement();
		fileName = newFileName;
	}
	error = 0;
}

void SliderCategoryFile::Rename(const string& newFileName) {
	fileName = newFileName;
}

int SliderCategoryFile::GetCategoryNames(vector<string>& outCategoryNames, bool append, bool unique) {
	unordered_set<string> existingNames;
	if (!append)
		outCategoryNames.clear();

	if (unique)
		existingNames.insert(outCategoryNames.begin(), outCategoryNames.end());

	for (auto &cn : categoriesInFile) {
		if (unique && existingNames.find(cn.first) != existingNames.end())
			continue;
		else if (unique)
			existingNames.insert(cn.first);

		outCategoryNames.push_back(cn.first);
	}
	return 0;
}

bool SliderCategoryFile::HasCategory(const string& queryCategoryName) {
	if (categoriesInFile.find(queryCategoryName) != categoriesInFile.end())
		return true;

	return false;
}

int SliderCategoryFile::GetAllCategories(vector<SliderCategory>& outAppendCategories) {
	int count = 0;
	bool add = true;
	for (auto &c : categoriesInFile) {
		add = true;
		for (auto& oc : outAppendCategories) {
			if (oc.GetName() == c.first) {
				oc.LoadCategory(c.second);
				add = false;
				break;
			}
		}
		if (!add)
			continue;

		outAppendCategories.emplace_back(c.second);
		count++;
	}
	return count;
}

int SliderCategoryFile::GetCategory(const string& categoryName, SliderCategory& outCategories) {
	if (categoriesInFile.find(categoryName) != categoriesInFile.end())
		outCategories.LoadCategory(categoriesInFile[categoryName]);
	else
		return 1;

	return 0;
}

int SliderCategoryFile::UpdateCategory(SliderCategory& inCategory) {
	if (categoriesInFile.find(inCategory.GetName()) != categoriesInFile.end()) {
		inCategory.WriteCategory(categoriesInFile[inCategory.GetName()]);
	}
	else {
		XMLElement* newElement = doc.NewElement("Category");
		XMLElement* element = root->InsertEndChild(newElement)->ToElement();
		element->SetAttribute("name", inCategory.GetName().c_str());
		inCategory.WriteCategory(element);
	}
	return 0;
}

int SliderCategoryFile::Save(){
	if (doc.SaveFile(fileName.c_str()) != XML_SUCCESS)
		return 1;

	return 0;
}
