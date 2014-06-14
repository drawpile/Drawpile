"use strict";
$(function() {

var _currentSession = null;
var _serverStatusUpdateTimer = 0;
var _sessionListUpdateTimer = 0;
var _sessionUpdateTimer = 0;

$("#changeTitleButton").click(function(ev) {
	ev.preventDefault();
	var textarea = $('<textarea rows=4></textarea>').val($("#serverTitle").data('value'));
	showDialog("Server title", textarea, function() {
		$.post("/api/status/", {title: textarea.val()}, function(result) {
			if(result.success !== true) {
				alert(result.errors);
			}
			updateServerStatus();
		});
		$("#serverTitle tbody").empty().append('<tr><td><span class="spinner"></span></td></tr>');
	});
});

function switchToSessionListView()
{
	$("#sessionView").hide();
	$("#sessionListView").show();
	_currentSession = null;
	updateServerStatus();
}

$("#sessionListView").on('click', '.session-link', function(ev) {
	ev.preventDefault();
	$("#sessionListView").hide();
	$("#sessionView").show();
	_currentSession = $(this).data("id");
	updateSessionView();
});

$("#sessionView .backButton").click(function(ev) {
	ev.preventDefault();
	switchToSessionListView();
});

$("#sessionView .killButton").click(function(ev) {
	ev.preventDefault();
	showDialog("Delete " + $("#sessionInfoTitle").text() + "?", $("#sessionDetailTitle").text(), function() {
		$.ajax({
			url: '/api/sessions/' + _currentSession,
			type: 'DELETE'
		}).done(function(result) {
			if(!result.success) {
				alert(result.errors);
			}
			switchToSessionListView();
		});
	});
});

$(".wallButton").click(function(ev) {
	ev.preventDefault();
	var textarea = $('<textarea rows=2></textarea>').val($("#serverTitle").data('value'));
	showDialog("Send message to all users", textarea, function() {
		if(_currentSession === null) {
			var url = "/api/wall";
		} else {
			var url = "/api/sessions/" + _currentSession + "/wall";
		}
		$.post(url, {message: textarea.val()}, function(result) {
			if(result.success !== true) {
				alert(result.errors);
			}
		});
	});
});

function updateSessionList()
{
	_sessionListUpdateTimer = 0;

	$("#sessionList>thead>tr:first-child>th").append('<span class="spinner"></span>');

	$.getJSON("/api/sessions/", function(sessions) {
		var tbody = $("#sessionList>tbody");
		tbody.empty();
		if(sessions.length === 0) {
			tbody.append('<tr class="empty"><td colspan="3">No sessions</td></tr>');
		} else {
			for(var i=0;i<sessions.length;i+=1) {
				var ses = sessions[i];
				var tr = $('<tr></tr>');
				tr.append('<td><a class="session-link" href="#" data-id="' + ses.id + '">' + ses.id + '</a></td>');
				tr.append($('<td></td>').append($('<a class="session-link" href="#" data-id="' + ses.id + '"></a>').text(ses.title || "(untitled)")));

				var flags = $("<td></td>");
				flags.append($('<span class="bubble' + (ses.userCount?'':' gray') + '"></span>').text(ses.userCount));
				_appendStatusFlags(flags, ses.flags);
				tr.append(flags)
				tbody.append(tr);
			}
		}
	}).always(function() { $("#sessionList .spinner").remove(); });
}

function updateSessionView() {
	_sessionUpdateTimer = 0;

	$.getJSON("/api/sessions/" + _currentSession, function(result) {
		$("#sessionInfoTitle").text("Session #" + result.id);
		$("#sessionDetailTitle").text(result.title);
		_appendStatusFlags($("#sessionStatus").empty(), result.flags);
		$("#sessionUserCount").text(result.users.length + "/" + result.maxUsers);

		var usertable = $("#sessionUserList tbody").empty();
		for(var ui=0;ui<result.users.length;ui+=1) {
			var user = result.users[ui];
			var tr = $("<tr></tr>");
			tr.append('<td>' + user.id + '</td>');
			tr.append($('<td></td>').text(user.name));
			var flags = $('<td></td>');
			_appendStatusFlags(flags, user.flags);
			tr.append(flags);
			usertable.append(tr);
		}
	});
}

function updateServerStatus() {
	_serverStatusUpdateTimer = 0;

	$.getJSON("/api/status/", function(result) {
		var newTitle = $("<tr></tr>");
		if(result.title.length===0) {
			newTitle.addClass("empty").append("<td>(not set)</td>");
		} else {
			newTitle.append($("<td></td>").text(result.title));
		}

		$("#serverTitle").data('value', result.title);
		$("#serverTitle tbody").empty().append(newTitle);

		$("#sessionCount").text(result.sessionCount + "/" + result.maxSessions);
		$("#totalUsers").text(result.totalUsers);
		_appendStatusFlags($("#serverFlags").empty(), result.flags);
	});
}

function _appendStatusFlags(to, flags)
{
	for(var i=0;i<flags.length;i+=1) {
		to.append('<span class="red bubble">' + flags[i] + '</span>');
	}
}

var _overlayOkCallback = null;
function showDialog(title, content, okCallback)
{
	$("#overlay-title").text(title);
	$("#overlay-content").empty().append(content);
	_overlayOkCallback = okCallback;
	$("#overlay").show();
}

function closeDialog()
{
	$("#overlay").hide();
	_overlayOkCallback = null;
}

$("#overlay .backButton").click(function(ev) {
	ev.preventDefault();
	closeDialog();
});

$("#overlay .okButton").click(function(ev) {
	ev.preventDefault();
	if(_overlayOkCallback() !== false) {
		closeDialog();
	}
});

updateServerStatus();
updateSessionList();

setInterval(function() {
	_serverStatusUpdateTimer += 1;
	_sessionListUpdateTimer += 1;
	_sessionUpdateTimer += 1;

	if(_serverStatusUpdateTimer>10) {
		updateServerStatus();
	}
	if(_sessionListUpdateTimer>2 && _currentSession===null) {
		updateSessionList();
	}
	if(_sessionUpdateTimer>=1 && _currentSession!==null) {
		updateSessionView();
	}
}, 1000);
});
