-- phpMyAdmin SQL Dump
-- version 2.6.2
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Sep 10, 2005 at 12:22 PM
-- Server version: 4.1.12
-- PHP Version: 5.0.4
-- 
-- Database: `mangos`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `creaturetemplate`
-- 
-- Creation: Sep 10, 2005 at 12:02 PM
-- Last update: Sep 10, 2005 at 12:02 PM
-- 

DROP TABLE IF EXISTS `creaturetemplate`;
CREATE TABLE IF NOT EXISTS `creaturetemplate` (
  `entryid` int(11) NOT NULL default '0',
  `modelid` int(11) NOT NULL default '0',
  `name` varchar(100) NOT NULL default '',
  `subname` varchar(100) NOT NULL default '',
  `maxhealth` int(5) NOT NULL default '0',
  `maxmana` int(5) NOT NULL default '0',
  `level` int(3) NOT NULL default '0',
  `faction` int(4) NOT NULL default '0',
  `flag` int(4) NOT NULL default '0',
  `scale` float(3,0) NOT NULL default '0',
  `speed` float NOT NULL default '0',
  `rank` int(1) NOT NULL default '0',
  `mindmg` float NOT NULL default '0',
  `maxdmg` float NOT NULL default '0',
  `minrangedmg` float NOT NULL default '0',
  `maxrangedmg` float NOT NULL default '0',
  `baseattacktime` int(4) NOT NULL default '0',
  `rangeattacktime` int(4) NOT NULL default '0',
  `slot1model` int(11) NOT NULL default '0',
  `slot1pos` int(11) NOT NULL default '1',
  `slot2model` int(11) NOT NULL default '0',
  `slot2pos` int(11) NOT NULL default '1',
  `slot3model` int(11) NOT NULL default '0',
  `slot3pos` int(11) NOT NULL default '1',
  `type` int(2) NOT NULL default '0',
  `mount` int(5) NOT NULL default '0',
  PRIMARY KEY  (`entryid`)
) TYPE=MyISAM;
