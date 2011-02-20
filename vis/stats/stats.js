(function() {
	function init()
	{
		var tgroups = new Object();

		var heads = document.getElementsByTagName("h2");
		for (var i = 0; i < heads.length; i++)
		{
			var h2 = heads[i];

			if (h2.className.indexOf("sect") < 0)
				continue; /* not a section header after all */

			var sect = document.getElementById("s"+h2.id);
			if (!sect)
				continue; /* missing section header */

			/* hide section contents by default */

			sect.style.display = "none";

			/* insert the folding toggle button */

			var btn = document.createElement("span");
			btn.className = "toggle";
			btn.appendChild(document.createTextNode("⊞ "));
			btn.addEventListener("click", toggle(sect), false);

			h2.insertBefore(btn, h2.firstChild);

			/* add to a toggle group too if possible */

			var p = h2.parentNode;
			if (p.className == 'tg')
			{
				var sects = tgroups[p.id];
				if (!sects) tgroups[p.id] = sects = new Array();
				sects.push(sect);
			}
		}

		for (var tg in tgroups)
		{
			var div = document.getElementById(tg);

			var btn1 = document.createElement("span");
			btn1.className = "toggle";
			btn1.appendChild(document.createTextNode("[open all]"));
			btn1.addEventListener("click", opengroup(tgroups[tg]), false);

			var btn2 = document.createElement("span");
			btn2.className = "toggle";
			btn2.appendChild(document.createTextNode("[close all]"));
			btn2.addEventListener("click", closegroup(tgroups[tg]), false);

			div.insertBefore(btn2, div.firstChild);
			div.insertBefore(document.createTextNode(" "), div.firstChild);
			div.insertBefore(btn1, div.firstChild);
		}
	}

	function toggle(sect)
	{
		return function() {
			if (sect.style.display == "none")
				sect.style.display = "block";
			else
				sect.style.display = "none";
		};
	}

	function opengroup(sects)
	{
		return function() {
			for (var i in sects)
				sects[i].style.display = "block";
		};
	}

	function closegroup(sects)
	{
		return function() {
			for (var i in sects)
				sects[i].style.display = "none";
		};
	}

	document.addEventListener("DOMContentLoaded", init, false);
})();
