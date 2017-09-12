// store tabs variable
var myTabs = document.querySelectorAll("ul.tabbar > li");

function myTabClicks(tabClickEvent) {
	tabClickEvent.preventDefault();

	var anchorReference = tabClickEvent.target;
	var activePaneId = anchorReference.getAttribute("href");
	var activePane = document.querySelector(activePaneId);

	if (activePane == undefined) {
		return;
	}

	for (var i = 0; i < myTabs.length; i++) {
		myTabs[i].classList.remove("active");
	}

	var clickedTab = tabClickEvent.currentTarget;

	clickedTab.classList.add("active");

	var myContentPanes = document.querySelectorAll(".tab-pane");

	for (i = 0; i < myContentPanes.length; i++) {
		myContentPanes[i].classList.remove("active");
	}

	activePane.classList.add("active");
}

for (i = 0; i < myTabs.length; i++) {
	myTabs[i].addEventListener("click", myTabClicks)
}
