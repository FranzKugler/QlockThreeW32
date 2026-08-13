$(document).ready(function () {
	$('.header').height($(window).height());
});

function bodyOnLoad() {
	getActualState();
}

function showForm(formName) {
    // Hide all forms
    document.getElementById('formDisplay').classList.add('hidden');
    document.getElementById('formColor').classList.add('hidden');
    document.getElementById('formTimezone').classList.add('hidden');
    
    // Show selected form
    document.getElementById('form' + formName).classList.remove('hidden');

	// Collapse navbar on smaller screens
	var navBar = document.querySelector('.navbar-collapse');
	if (navBar.classList.contains('show')) {
		var toggler = document.querySelector('.navbar-toggler');
		toggler.click();
	}
}


var colorPicker = new iro.ColorPicker('#picker', {
	borderWidth : 6,
	wheelLightness: false
});

colorPicker.on('color:change', setColor);

function getActualState() {
	$.ajax({
		dataType: 'jsonp',
		data: "data=yeah",
		jsonp: 'callback',
		url: '/currentState?callback=?',
		success: function (data) {
			
			// display section
			switch (data.display) {
				case 0:
					$('#displayRadio0').attr('checked', true);
					break;
				case 1:
					$('#displayRadio1').attr('checked', true);
					break;
				case 2:
					$('#displayRadio2').attr('checked', true);
					break;
				case 3:
					$('#displayRadio3').attr('checked', true);
					break;
				case 4:
					$('#displayRadio4').attr('checked', true);
					break;
				case 6:
					$('#displayRadio6').attr('checked', true);
					break;
			}

			// color section
			colorPicker.color.set({h : data.hue, s : data.sat, l : data.lum});

			// rest of configuration
			$('#lumAutomaticCheck').prop('checked', data.automaticLum);
			$('#languageSelect').val(data.language);
			$('#cornerDirectionSelect').val(data.cornerDirection);
			$('#cornerColorSelect').val(data.cornerColor);

			// timezone
			$('#ntpServer').val(data.ntpServer);
			$('#tzDst').prop('checked', data.useDs);
			$('#tzName').val(data.tzName);
			$('#tzWeek').val(data.tzWeek);
			$('#tzDay').val(data.tzDoW);
			$('#tzMonth').val(data.tzMonth);			
			$('#tzHour').val(data.tzHour);
			$('#tzOffset').val(data.tzOffset);
			$('#tzDsName').val(data.tzDsName);
			$('#tzDsWeek').val(data.tzDsWeek);
			$('#tzDsDay').val(data.tzDsDoW);
			$('#tzDsMonth').val(data.tzDsMonth);			
			$('#tzDsHour').val(data.tzDsHour);
			$('#tzDsOffset').val(data.tzDsOffset);

			// Based on UseDs - lets activate / de-activate some entries
			setDst(data.useDs);
		}
	});
}

function setDisplay(value) {
	$.ajax({
		type: 'post',
		url: '/display',
		data: JSON.stringify({ display: value }),
		dataType: 'json',
		contentType: 'application/json; charset=utf-8'
	}).done(function (response) {
	})
}

function setAutoLuminance() {
	$.ajax({
		type: 'post',
		url: '/autoluminance',
		data: JSON.stringify({automaticLum: document.getElementById("lumAutomaticCheck").val() == 'on' ? 1 : 0 }),
		dataType: 'json',
		contentType: 'application/json; charset=utf-8'
	}).done(function (response) {
	})
}

function setColor(color) {
	$.ajax({
		type: 'post',
		url: '/color',
		data: JSON.stringify({ hue: color.hsl['h'], sat: color.hsl['s'], lum: color.hsl['l'] }),
		dataType: 'json',
		contentType: 'application/json; charset=utf-8'
	}).done(function (response) {
	})
}

function setConfiguration() {
	$.ajax({
		type: 'post',
		url: '/configuration',
		data: JSON.stringify({
			language: $("#languageSelect option:selected").val(),
			cornerColor: $("#cornerColorSelect option:selected").val(),
			cornerDirection: $("#cornerDirectionSelect option:selected").val()
		}),
		dataType: 'json',
		contentType: 'application/json; charset=utf-8'
	}).done(function (response) {
	})
}

function setDst(dst) {
	if (dst) {
		document.getElementById("tzWeek").removeAttribute('disabled');
		document.getElementById("tzDay").removeAttribute('disabled');
		document.getElementById("tzMonth").removeAttribute('disabled');
		document.getElementById("tzHour").removeAttribute('disabled');
		document.getElementById("tzDsName").removeAttribute('disabled');
		document.getElementById("tzDsWeek").removeAttribute('disabled');
		document.getElementById("tzDsDay").removeAttribute('disabled');
		document.getElementById("tzDsMonth").removeAttribute('disabled');
		document.getElementById("tzDsHour").removeAttribute('disabled');
		document.getElementById("tzDsOffset").removeAttribute('disabled');
	} else {
		document.getElementById("tzWeek").setAttribute('disabled', 'true');
		document.getElementById("tzDay").setAttribute('disabled', 'true');
		document.getElementById("tzMonth").setAttribute('disabled', 'true');
		document.getElementById("tzHour").setAttribute('disabled', 'true');
		document.getElementById("tzDsName").setAttribute('disabled', 'true');
		document.getElementById("tzDsWeek").setAttribute('disabled', 'true');
		document.getElementById("tzDsDay").setAttribute('disabled', 'true');
		document.getElementById("tzDsMonth").setAttribute('disabled', 'true');
		document.getElementById("tzDsHour").setAttribute('disabled', 'true');
		document.getElementById("tzDsOffset").setAttribute('disabled', 'true');
	}
}

function setTimezone() {
	setDst(document.getElementById("tzDst").checked)
	$.ajax({
		type: 'post',
		url: '/timezone',
		data: JSON.stringify({
			ntpServer : $('#ntpServer').val(),
			useDs : document.getElementById('tzDst').checked ? 1 : 0,
			tzName : $('#tzName').val(),
			tzWeek : $('#tzWeek').val(),
			tzDoW : $('#tzDay').val(),
			tzMonth :$('#tzMonth').val(),			
			tzHour : $('#tzHour').val(),
			tzOffset : $('#tzOffset').val(),
			tzDsName : $('#tzDsName').val(),
			tzDsWeek : $('#tzDsWeek').val(),
			tzDsDoW : $('#tzDsDay').val(),
			tzDsMonth : $('#tzDsMonth').val(),			
			tzDsHour : $('#tzDsHour').val(),
			tzDsOffset : $('#tzDsOffset').val()			
		}),
		dataType: 'json',
		contentType: 'application/json; charset=utf-8'
	}).done(function (response) {
	})
}